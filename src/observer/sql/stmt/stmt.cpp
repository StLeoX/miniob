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

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "rc.h"
#include "sql/stmt/stmt.h"
#include "common/log/log.h"
#include "sql/parser/parse_defs.h"
#include "sql/stmt/insert_stmt.h"
#include "sql/stmt/delete_stmt.h"
#include "sql/stmt/select_stmt.h"
#include "sql/stmt/update_stmt.h"
#include "storage/common/field_meta.h"
#include "util/util.h"


RC Stmt::create_stmt(Db *db, Query &query, Stmt *&stmt)
{
  stmt = nullptr;

  switch (query.flag) {
  case SCF_INSERT: {
      return InsertStmt::create(db, query.sstr.insertion, stmt);
    }
  case SCF_DELETE: {
      return DeleteStmt::create(db, query.sstr.deletion, stmt);   
    }
  case SCF_SELECT: {
    return SelectStmt::create(db, query.sstr.selection, stmt);
  }
  // case SCF_UPDATE: {
  //   return UpdateStmt::create(db, query.sstr.update, stmt);
  // }
  default: {
      // LOG_WARN("unknown query command");
    }
    break;
  }
  return RC::UNIMPLENMENT;
}

bool Stmt::check_type(AttrType t1, AttrType t2)
{
  if (t1 == AttrType::DATES && t2 != AttrType::DATES && t2 != AttrType::NULLS) {
    return false;
  }
  if (t1 != AttrType::DATES && t2 == AttrType::DATES && t1 != AttrType::NULLS) {
    return false;
  }
  return true;
}

int Stmt::char_to_int(const char *s)
{
  int ans = 0;
  int i = 0;
  while (isdigit(s[i])) {
    ans = ans * 10 + s[i] - '0';
    i++;
  }
  return ans;
}

float Stmt::char_to_float(const char *s)
{
  float ans = 0;
  int i = 0;
  if (isdigit(s[i])) {
    while (isdigit(s[i])) {
      ans = ans * 10 + s[i] - '0';
      i++;
    }
    if (s[i] == '.') {
      i++;
      float tp = 0;
      int j = 1;
      while (isdigit(s[i])) {
        tp = tp * 10 + s[i] - '0';
        i++;
        j *= 10;
      }
      ans += tp / j;
    }
  }
  return ans;
}

/**
 * cast value to field type
 * 无需处理TEXTS和CHARS类型的转换，因为本质上不存在TEXTS类型
 **/
bool Stmt::convert_type(const FieldMeta *field, Value *value)
{
  if (value->type == field->type()) {
    return true;
  }
  if (value->type == AttrType::NULLS) {
    return true;
  }
  AttrType type = field->type();
  // char *data = new char(field->len());
  void *data = malloc(field->len());
  memset(data, 0, field->len());
  if (type == AttrType::INTS && value->type == AttrType::FLOATS) {
    // float -> int
    int tp = *(float *)value->data + 0.5;
    memcpy(data, &tp, sizeof(tp));
    value->type = AttrType::INTS;
    // LOG_DEBUG("%f converts to %d", tp, );
  } else if (type == AttrType::FLOATS && value->type == AttrType::INTS) {
    // int -> float
    float tp = *(int *)value->data;
    memcpy(data, &tp, sizeof(tp));
    value->type = AttrType::FLOATS;
    // LOG_DEBUG("%d converts to %f", (int)tp, tp);
  } else if (type == AttrType::INTS && value->type == AttrType::CHARS) {
    // char -> int
    char *s = (char *)value->data;
    int tp = char_to_int(s);
    memcpy(data, &tp, sizeof(tp));
    value->type = AttrType::INTS;
    // LOG_DEBUG("%s converts to %d", (char *)value->data, ans);
  } else if (type == AttrType::FLOATS && value->type == AttrType::CHARS) {
    char *s = (char *)value->data;
    float tp = char_to_float(s);
    memcpy(data, &tp, sizeof(tp));
    value->type = AttrType::FLOATS;
    // LOG_DEBUG("%s converts to %f", (char *)value->data, ans);
  } else if (type == AttrType::CHARS && value->type == AttrType::INTS) {
    // int -> chars
    std::string s = std::to_string(*(int*)value->data);
    memcpy(data, s.c_str(), s.size() + 1);
    value->type = AttrType::CHARS;
    // sprintf(str, "%d", *(int *)value->data);
    // LOG_DEBUG("%d converts to %s", *(int *)value->data, str);
  } else if (type == AttrType::CHARS && value->type == AttrType::FLOATS) {
    // char *str = strdup(std::to_string(*(float*)value->data).c_str());
    // for (int i = strlen(str) - 1; i > 0; i--) {
    //   if (str[i] == '0') {
    //     str[i] = '\0';
    //   } else {
    //     break;
    //   }
    // }
    std::string s = double2string(*(float*)value->data);
    memcpy(data, s.c_str(), s.size() + 1);
    value->type = AttrType::CHARS;
  } else {
    return false;
  }
  free(value->data);
  value->data = data;
  return true;
}

RC Stmt::get_table_and_field(Db *db, Table *default_table, std::unordered_map<std::string, Table *> *tables,
		       const RelAttr &attr, Table *&table, const FieldMeta *&field)
{
  if (common::is_blank(attr.relation_name)) {
    table = default_table;
  } else if (nullptr != tables) {
    auto iter = tables->find(std::string(attr.relation_name));
    if (iter != tables->end()) {
      table = iter->second;
    }
  } else {
    table = db->find_table(attr.relation_name);
  }
  if (nullptr == table) {
    LOG_WARN("No such table: attr.relation_name: %s", attr.relation_name);
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }

  field = table->table_meta().field(attr.attribute_name);
  if (nullptr == field) {
    LOG_WARN("no such field in table: table %s, field %s", table->name(), attr.attribute_name);
    table = nullptr;
    return RC::SCHEMA_FIELD_NOT_EXIST;
  }

  return RC::SUCCESS;
}
