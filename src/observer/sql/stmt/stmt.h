/* Copyright (c) 2021 Xie Meiyi(xiemeiyi@hust.edu.cn) and OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by Wangyunlai on 2022/5/22.
//

#pragma once

#include "rc.h"
#include "common/log/log.h"
#include "common/lang/string.h"
#include "sql/parser/parse_defs.h"
#include "storage/common/db.h"
#include "storage/common/field_meta.h"
#include "storage/common/table.h"

class Db;

enum class StmtType
{
  SELECT,
  INSERT,
  UPDATE,
  DELETE,
  CREATE_TABLE,
  DROP_TABLE,
  CREATE_INDEX,
  DROP_INDEX,
  SYNC,
  SHOW_TABLES,
  DESC_TABLE,
  BEGIN,
  COMMIT,
  ROLLBACK,
  LOAD_DATA,
  HELP,
  EXIT,

  PREDICATE,
};

class Stmt 
{
public:

  Stmt() = default;
  virtual ~Stmt() = default;

  virtual StmtType type() const = 0;

public:
  static RC create_stmt(Db *db, Query &query, Stmt *&stmt);
  static bool convert_type(const FieldMeta *field, Value *value);
  static RC get_table_and_field(Db *db, Table *default_table, std::unordered_map<std::string, Table *> *tables,
                                const RelAttr &attr, Table *&table, const FieldMeta *&field);
  static bool check_type(AttrType t1, AttrType t2);
  static int char_to_int(const char *s);
  static float char_to_float(const char *s);

private:
};

