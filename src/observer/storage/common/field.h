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
// Created by Wangyunlai on 2022/07/05.
//

#pragma once

#include <string>
#include "storage/common/table.h"
#include "storage/common/field_meta.h"

class Field
{
public:
  Field() = default;
  Field(const Table *table, const FieldMeta *field) : table_(table), field_(field)
  {}

  const Table *table() const { return table_; }
  FieldMeta *meta() { return const_cast<FieldMeta *>(field_); }
  const FieldMeta *metac() const { return field_; }
  FieldMeta meta_copy() const { return *field_; }

  AttrType attr_type() const
  {
    return field_->type();
  }

  const char *table_name() const { return table_->name(); }
  const char *field_name() const { return field_->name(); }

  void set_table(const Table *table)
  {
    this->table_ = table;
  }
  void set_field(const FieldMeta *field)
  {
    this->field_ = field;
  }

  bool has_table() const { return table_ != nullptr; }

  void set_aggr(AggreType type) {
    aggr_type_ = type;
    // const_cast<FieldMeta *>(field_)->set_aggr_type(type);
  }
  const std::string &aggr_str() const { return aggr_str_; }
  void set_aggr_str(std::string cnt) { aggr_str_ = cnt; }
  AggreType aggr_type() const { return aggr_type_; }
  void set_table(Table *table) { table_ = table; }
  bool has_field() const { return field_ != nullptr; }
  void set_print_table() { print_table_ = true; }
  bool should_print_table() const { return print_table_; }
private:
  const Table *table_ = nullptr;
  const FieldMeta *field_ = nullptr;
  AggreType aggr_type_ = A_NO;
  std::string aggr_str_;
  // for count(1), avg(1.111)
  bool print_table_ = false;
};
