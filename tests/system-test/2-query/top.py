###################################################################
#           Copyright (c) 2016 by TAOS Technologies, Inc.
#                     All rights reserved.
#
#  This file is proprietary and confidential to TAOS Technologies.
#  No part of this file may be reproduced, stored, transmitted,
#  disclosed or used in any form or by any means other than as
#  expressly provided by the written permission from Jianhui Tao
#
###################################################################

# -*- coding: utf-8 -*-

from util.log import *
from util.cases import *
from util.sql import *


class TDTestCase:
    def init(self, conn, logSql):
        tdLog.debug("start to execute %s" % __file__)
        tdSql.init(conn.cursor())

        self.rowNum = 10
        self.ts = 1537146000000
        
    def run(self):
        tdSql.prepare()

        

        tdSql.execute('''create table test(ts timestamp, col1 tinyint, col2 smallint, col3 int, col4 bigint, col5 float, col6 double, 
                    col7 bool, col8 binary(20), col9 nchar(20), col11 tinyint unsigned, col12 smallint unsigned, col13 int unsigned, col14 bigint unsigned) tags(loc nchar(20))''')
        tdSql.execute("create table test1 using test tags('beijing')")
        for i in range(self.rowNum):
            tdSql.execute("insert into test1 values(%d, %d, %d, %d, %d, %f, %f, %d, 'taosdata%d', '涛思数据%d', %d, %d, %d, %d)" 
                        % (self.ts + i, i + 1, i + 1, i + 1, i + 1, i + 0.1, i + 0.1, i % 2, i + 1, i + 1, i + 1, i + 1, i + 1, i + 1))
                                

        # top verifacation 
        tdSql.error("select top(ts, 10) from test")
        tdSql.error("select top(col1, 0) from test")
        tdSql.error("select top(col1, 101) from test")
        tdSql.error("select top(col2, 0) from test")
        tdSql.error("select top(col2, 101) from test")
        tdSql.error("select top(col3, 0) from test")
        tdSql.error("select top(col3, 101) from test")
        tdSql.error("select top(col4, 0) from test")
        tdSql.error("select top(col4, 101) from test")
        tdSql.error("select top(col5, 0) from test")
        tdSql.error("select top(col5, 101) from test")
        tdSql.error("select top(col6, 0) from test")
        tdSql.error("select top(col6, 101) from test")        
        tdSql.error("select top(col7, 10) from test")        
        tdSql.error("select top(col8, 10) from test")
        tdSql.error("select top(col9, 10) from test")
        tdSql.error("select top(col11, 0) from test")
        tdSql.error("select top(col11, 101) from test")
        tdSql.error("select top(col12, 0) from test")
        tdSql.error("select top(col12, 101) from test")
        tdSql.error("select top(col13, 0) from test")
        tdSql.error("select top(col13, 101) from test")
        tdSql.error("select top(col14, 0) from test")
        tdSql.error("select top(col14, 101) from test")

        tdSql.query("select top(col1, 2) from test")
        tdSql.checkRows(2)
        tdSql.checkEqual(tdSql.queryResult,[(9,),(10,)])
        tdSql.query("select top(col2, 2) from test")
        tdSql.checkRows(2)
        tdSql.checkEqual(tdSql.queryResult,[(9,),(10,)])
        tdSql.query("select top(col3, 2) from test")
        tdSql.checkRows(2)
        tdSql.checkEqual(tdSql.queryResult,[(9,),(10,)])
        tdSql.query("select top(col4, 2) from test")
        tdSql.checkRows(2)
        tdSql.checkEqual(tdSql.queryResult,[(9,),(10,)])
        tdSql.query("select top(col11, 2) from test")
        tdSql.checkRows(2)
        tdSql.checkEqual(tdSql.queryResult,[(9,),(10,)])
        tdSql.query("select top(col12, 2) from test")
        tdSql.checkRows(2)
        tdSql.checkEqual(tdSql.queryResult,[(9,),(10,)])
        tdSql.query("select top(col13, 2) from test")
        tdSql.checkRows(2)
        tdSql.checkEqual(tdSql.queryResult,[(9,),(10,)])
        tdSql.query("select top(col14, 2) from test")
        tdSql.checkRows(2)
        tdSql.checkEqual(tdSql.queryResult,[(9,),(10,)])
        tdSql.query("select ts,top(col1, 2),ts from test1")
        tdSql.checkRows(2)
        tdSql.query("select top(col14, 100) from test")
        tdSql.checkRows(10)
        tdSql.query("select ts,top(col1, 2),ts from test group by tbname")
        tdSql.checkRows(2)
        tdSql.query('select top(col2,1) from test interval(1y) order by col2')
        tdSql.checkData(0,0,10)
        
        tdSql.error("select * from test where bottom(col2,1)=1")
        tdSql.error("select top(col14, 0) from test;")
    def stop(self):
        tdSql.close()
        tdLog.success("%s successfully executed" % __file__)

tdCases.addWindows(__file__, TDTestCase())
tdCases.addLinux(__file__, TDTestCase())
