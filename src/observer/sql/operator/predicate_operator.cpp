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
// Created by WangYunlai on 2022/6/27.
//

#include "common/log/log.h"
#include "sql/expr/expression.h"
#include "sql/executor/execute_stage.h"
#include "util/util.h"
#include "sql/operator/predicate_operator.h"
#include "sql/parser/parse_defs.h"
#include "storage/record/record.h"
#include "sql/stmt/filter_stmt.h"
#include "storage/common/field.h"
#include <cstring>
#include <vector>

RC PredicateOperator::open()
{
  if (children_.size() != 1) {
    LOG_WARN("predicate operator must has one child");
    return RC::INTERNAL;
  }

  return children_[0]->open();
}

RC PredicateOperator::next()
{
  RC rc = RC::SUCCESS;
  Operator *oper = children_[0];

  while (RC::SUCCESS == (rc = oper->next())) {
    Tuple *tuple = oper->current_tuple();
    if (nullptr == tuple) {
      rc = RC::INTERNAL;
      LOG_WARN("failed to get tuple from operator");
      break;
    }

    if (do_predicate(static_cast<RowTuple &>(*tuple))) {
      return rc;
    }
  }
  return rc;
}

RC PredicateOperator::close()
{
  children_[0]->close();
  return RC::SUCCESS;
}

Tuple *PredicateOperator::current_tuple()
{
  return children_[0]->current_tuple();
}

bool PredicateOperator::do_predicate(RowTuple &tuple)
{
  if (filter_stmt_ == nullptr || filter_stmt_->filter_units().empty()) {
    return true;
  }

  for (const FilterUnit *filter_unit : filter_stmt_->filter_units()) {
    Expression *left_expr = filter_unit->left();
    Expression *right_expr = filter_unit->right();
    CompOp comp = filter_unit->comp();
    TupleCell left_cell;
    TupleCell right_cell;
    left_expr->get_value(tuple, left_cell);
    // note: 应当能算出来外面的exists的已经补全的左部
    LOG_DEBUG("exists? %d \n",comp == VALUE_EXISTS || comp == VALUE_NOT_EXISTS);
    std::stringstream  ss;
    left_cell.to_string(ss);
    LOG_DEBUG("left: %s \n", ss.str().c_str());
    right_expr->get_value(tuple, right_cell);

    // NULL COMPARE
    // TODO: type check
    bool filter_result = false;
    if (comp == VALUE_IN) {
      ValueExpr *right_value_expr = dynamic_cast<ValueExpr *>(right_expr);
      filter_result = right_value_expr->pretable()->in(left_cell);
    } else if (comp == VALUE_NOT_IN) {
      ValueExpr *right_value_expr = dynamic_cast<ValueExpr *>(right_expr);
      filter_result = right_value_expr->pretable()->not_in(left_cell);
    }else if (comp == VALUE_EXISTS) {
      ValueExpr *right_value_expr = dynamic_cast<ValueExpr *>(right_expr);
      filter_result = right_value_expr->pretable()->exists(left_cell);
    } else if (comp == VALUE_NOT_EXISTS) {
      ValueExpr *right_value_expr = dynamic_cast<ValueExpr *>(right_expr);
      filter_result = right_value_expr->pretable()->not_exists(left_cell);
    } else if (left_cell.attr_type() == UNDEFINED || right_cell.attr_type() == UNDEFINED) {
      return false;
    } else if (left_cell.attr_type() == AttrType::NULLS && right_cell.attr_type() == AttrType::NULLS &&
               comp == IS_EQUAL) {
      filter_result = true;
    } else if (left_cell.attr_type() != AttrType::NULLS && right_cell.attr_type() == AttrType::NULLS &&
               comp == IS_NOT_EQUAL) {
      filter_result = true;
    } else if (left_cell.attr_type() == AttrType::NULLS || right_cell.attr_type() == AttrType::NULLS) {
      return false;
    } else if (comp == STR_LIKE) {
      filter_result = string_like(left_cell.data(), right_cell.data());
    } else if (comp == STR_NOT_LIKE) {
      filter_result = !string_like(left_cell.data(), right_cell.data());
    } else {
      const int compare = left_cell.compare(right_cell);
      switch (comp) {
        case EQUAL_TO: {
          filter_result = (0 == compare);
        } break;
        case LESS_EQUAL: {
          filter_result = (compare <= 0);
        } break;
        case NOT_EQUAL: {
          filter_result = (compare != 0);
        } break;
        case LESS_THAN: {
          filter_result = (compare < 0);
        } break;
        case GREAT_EQUAL: {
          filter_result = (compare >= 0);
        } break;
        case GREAT_THAN: {
          filter_result = (compare > 0);
        } break;
        default: {
          LOG_WARN("invalid compare type: %d", comp);
        } break;
      }
    }
    if (!filter_result) {
      return false;
    }
  }
  return true;
}

// int PredicateOperator::tuple_cell_num() const
// {
//   return children_[0]->tuple_cell_num();
// }
// RC PredicateOperator::tuple_cell_spec_at(int index, TupleCellSpec &spec) const
// {
//   return children_[0]->tuple_cell_spec_at(index, spec);
// }
