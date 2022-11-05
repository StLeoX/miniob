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

#include "sql/stmt/update_stmt.h"
#include "common/log/log.h"
#include "sql/parser/parse_defs.h"
#include "sql/stmt/filter_stmt.h"
#include "storage/common/db.h"
#include "storage/common/field_meta.h"
#include "storage/common/table.h"
#include <string>
#include <unordered_map>
#include <vector>

RC UpdateStmt::create(Db *db, Updates &update, Stmt *&stmt)
{
  const char *table_name = update.relation_name;
  if (nullptr == db || nullptr == table_name) {
    LOG_WARN("invalid argument. db=%p, table_name=%p", db, table_name);
    return RC::INVALID_ARGUMENT;
  }

  Table *table = db->find_table(table_name);
  if (nullptr == table) {
    LOG_WARN("no such table. db=%s, table_name=%s", db->name(), table_name);
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }

  std::vector<Value *> values;
  std::vector<const FieldMeta *> metas;
  for (int i = 0; i < update.attribute_num; i++) {
    Value &value = update.values[i];
    const char *attr_name = update.attributes[i];

    if (value.type == DATES && *(int *)value.data == -1) {
      return RC::RECORD_INVALID_KEY;
    }

    const TableMeta &meta = table->table_meta();
    const FieldMeta *fmeta = meta.field(attr_name);
    if (fmeta == nullptr) {
      return RC::SCHEMA_FIELD_MISSING;
    }
    if (!fmeta->nullable() && value.type == AttrType::NULLS) {
      return RC::SCHEMA_FIELD_TYPE_MISMATCH;
    }
    if (value.type != SELECTS && fmeta->type() != value.type &&
        !convert_type(fmeta, &value)) {
      return RC::SCHEMA_FIELD_TYPE_MISMATCH;
    }
    values.push_back(&update.values[i]);
    metas.push_back(fmeta);
  }

  UpdateStmt *update_stmt = new UpdateStmt();
  update_stmt->table_ = table;
  update_stmt->condition_num_ = update.condition_num;
  update_stmt->conditions_ = update.conditions;
  update_stmt->attr_metas_.swap(metas);
  update_stmt->values_.swap(values);
  stmt = update_stmt;
  return RC::SUCCESS;
}

// for (int i = 0; i < condition_num; i++) {
  //   Condition &c = conditions[i];
  //   if (!c.left_is_attr && !c.right_is_attr) {
  //     return RC::INVALID_ARGUMENT;
  //   }
  //   if (c.left_is_attr) {
  //     const FieldMeta *field_meta = meta.field(c.left_attr.attribute_name);
  //     if (field_meta == nullptr) {
  //       return RC::SCHEMA_FIELD_NAME_ILLEGAL;
  //     }
  //     if (c.right_value.type == DATES && *(int*)c.right_value.data == -1) {
  //       return RC::INVALID_ARGUMENT;
  //     }
  //     // printf("%d\n", c.right_value.type);
  //     if (c.right_value.type != field_meta->type() &&
  //         !convert_type(field_meta->type(), &c.right_value)) {
  //       return RC::SCHEMA_FIELD_TYPE_MISMATCH;
  //     }
  //   } else {
  //     const FieldMeta *field_meta = meta.field(c.right_attr.attribute_name);
  //     if (field_meta == nullptr) {
  //       return RC::SCHEMA_FIELD_NAME_ILLEGAL;
  //     }
  //     if (c.left_value.type == DATES && *(int*)c.left_value.data == -1) {
  //       return RC::INVALID_ARGUMENT;
  //     }
  //     // printf("%d\n", c.right_value.type);
  //     if (c.left_value.type != field_meta->type() &&
  //         !convert_type(field_meta->type(), &c.left_value)) {
  //       return RC::SCHEMA_FIELD_TYPE_MISMATCH;
  //     }
  //   }
  // }
