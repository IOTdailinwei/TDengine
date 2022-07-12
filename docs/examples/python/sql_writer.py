import logging
import taos


class SQLWriter:
    log = logging.getLogger("SQLWriter")

    def __init__(self, max_batch_size):
        self._buffered_count = 0
        self._max_batch_size = max_batch_size
        self._tb_values = {}
        self._tb_tags = {}
        self._conn = self.get_connection()
        self._max_sql_length = self.get_max_sql_length()
        self._conn.execute("USE test")

    def get_max_sql_length(self):
        rows = self._conn.query("SHOW variables").fetch_all()
        for r in rows:
            name = r[0]
            if name == "maxSQLLength":
                return int(r[1])

    def get_connection(self):
        return taos.connect(host="localhost", user="root", password="taosdata", port=6030)

    def process_line(self, line: str):
        """
        :param line: tbName,ts,current,voltage,phase,location,groupId
        """
        self._buffered_count += 1
        ps = line.split(",")
        table_name = ps[0]
        value = '(' + ",".join(ps[1:-2]) + ') '
        if table_name in self._tb_values:
            self._tb_values[table_name] += value
        else:
            self._tb_values[table_name] = value

        if table_name not in self._tb_tags:
            location = ps[-2]
            group_id = ps[-1]
            tag_value = f"('{location}',{group_id})"
            self._tb_tags[table_name] = tag_value

        if self._buffered_count == self._max_batch_size:
            self.flush()

    def flush(self):
        """
        Assemble INSERT statement and execute it.
        When the sql length grows close to MAX_SQL_LENGTH, the sql will be executed immediately, and a new INSERT statement will be created.
        In case of "Table does not exit" exception, tables in the sql will be created and the sql will be re-executed.
        """
        sql = "INSERT INTO "
        sql_len = len(sql)
        buf = []
        for tb_name, values in self._tb_values.items():
            q = tb_name + " VALUES " + values
            if sql_len + len(q) >= self._max_sql_length:
                sql += " ".join(buf)
                self.execute_sql(sql)
                sql = "INSERT INTO "
                sql_len = len(sql)
                buf = []
            buf.append(q)
            sql_len += len(q)
        sql += " ".join(buf)
        self.execute_sql(sql)
        self._tb_values.clear()
        self._buffered_count = 0

    def execute_sql(self, sql):
        try:
            self._conn.execute(sql)
        except taos.Error as e:
            error_code = e.errno & 0xffff
            # Table does not exit
            if error_code == 0x362 or error_code == 0x218:
                self.create_tables()
            else:
                raise e

    def create_tables(self):
        sql = "CREATE TABLE "
        for tb in self._tb_values.keys():
            tag_values = self._tb_tags[tb]
            sql += "IF NOT EXISTS " + tb + " USING meters TAGS " + tag_values + " "
        self._conn.execute(sql)

    @property
    def buffered_count(self):
        return self._buffered_count
