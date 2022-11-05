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

#include "sql/expr/tuple.h"
#include "sql/executor/execute_stage.h"
#include "sql/parser/parse_defs.h"
// #include "expression.h"


RC FieldExpr::get_value(const Tuple &tuple, TupleCell &cell) const
{
  return tuple.find_cell(field_, cell);
}

bool FieldExpr::operator==(const FieldExpr &rhs) const
{
  return strcmp(field_.table_name(), rhs.table_name()) == 0 &&
         strcmp(field_.field_name(), rhs.field_name()) == 0;
}

bool FieldExpr::operator!=(const FieldExpr &rhs) const
{
  return !(rhs == *this);
}

RC ValueExpr::get_value(const Tuple &tuple, TupleCell & cell) const
{
  if (pretable_ != nullptr) {
    if (pretable_->tuple_num() == 0) {
      cell.set_type(UNDEFINED);
    } else {
      cell = pretable_->get_first_cell();
    }
  } else {
    cell = tuple_cell_;
  }
  return RC::SUCCESS;
}
