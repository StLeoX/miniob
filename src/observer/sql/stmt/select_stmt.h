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
// Created by Wangyunlai on 2022/6/5.
//

#pragma once

#include <unordered_set>
#include <vector>

#include "rc.h"
#include "sql/parser/parse_defs.h"
#include "sql/stmt/stmt.h"
#include "storage/common/field.h"

class FieldMeta;
class FilterStmt;
class Db;
class Table;

class OrderByField {
 public:
  Table* table;
  const FieldMeta* field_meta;
  int is_desc;
};

class SelectStmt : public Stmt
{
public:

  SelectStmt() = default;
  ~SelectStmt() override;

  StmtType type() const override { return StmtType::SELECT; }
public:
  static RC create(Db *db, Selects *select_sql, Stmt *&stmt);
  static RC create(Db *db, Selects *select_sql, Stmt *&stmt, std::unordered_set<Table *> &parent_tables);

public:
  const std::vector<Table *> &tables() const { return tables_; }
  const std::vector<Field> &query_fields() const { return query_fields_; }
  std::vector<Field> &query_fields() { return query_fields_; }
  const std::vector<Field> &groupby_fields() const { return groupby_fields_; }
  std::vector<Field> &groupby_fields() { return groupby_fields_; }
  const std::vector<OrderByField> &order_by_fields() const { return order_by_fields_; }
  std::vector<OrderByField> &order_by_fields() { return order_by_fields_; }
  FilterStmt *filter_stmt() const { return filter_stmt_; }
  int aggregate_num() const { return aggregate_num_; }
  Condition *having_conditions() { return having_conditions_; }
  int having_condition_num() { return having_condition_num_; }
  int query_num() { return query_num_; }
  // void add_parent_tables(std::unordered_set<Table *> tables) { parent_tables_ = tables; }
  // std::unordered_set<Table *> parent_tables() { return parent_tables_; }

private:
  std::vector<Field> query_fields_;
  std::vector<Field> groupby_fields_;
  // std::vector<Field> having_fields_;
  Condition *having_conditions_;
  int having_condition_num_;
  int query_num_;
  std::vector<Table *> tables_;
  // std::unordered_set<Table *> parent_tables_;
  FilterStmt *filter_stmt_ = nullptr;
  int aggregate_num_ = 0;
  /**
   * Order by these fields.
   * No need to order if this vector has zero length
   */
  std::vector<OrderByField> order_by_fields_;
};
