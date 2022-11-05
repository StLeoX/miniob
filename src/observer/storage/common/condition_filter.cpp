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
// Created by Meiyi & Wangyunlai on 2021/5/7.
//

#include <stddef.h>
#include <math.h>
#include "condition_filter.h"
#include "sql/parser/parse_defs.h"
#include "storage/record/record_manager.h"
#include "sql/stmt/stmt.h"
#include "common/log/log.h"
#include "util/util.h"
#include "storage/common/table.h"

using namespace common;

ConditionFilter::~ConditionFilter()
{}

DefaultConditionFilter::DefaultConditionFilter()
{
  left_.is_attr = false;
  left_.attr_length = 0;
  left_.attr_offset = 0;
  left_.value = nullptr;

  right_.is_attr = false;
  right_.attr_length = 0;
  right_.attr_offset = 0;
  right_.value = nullptr;
}

DefaultConditionFilter::~DefaultConditionFilter()
{}

RC DefaultConditionFilter::init(
    const ConDesc &left, const ConDesc &right, AttrType type_left, AttrType type_right, CompOp comp_op)
{
  if (type_left < CHARS || type_right > FLOATS) {
    LOG_ERROR("Invalid condition with unsupported attribute type: %d", type_left);
    return RC::INVALID_ARGUMENT;
  }
  if (type_right < CHARS || type_right > FLOATS) {
    LOG_ERROR("Invalid condition with unsupported attribute type: %d", type_right);
    return RC::INVALID_ARGUMENT;
  }

  if (comp_op < EQUAL_TO || comp_op >= NO_OP) {
    LOG_ERROR("Invalid condition with unsupported compare operation: %d", comp_op);
    return RC::INVALID_ARGUMENT;
  }

  left_ = left;
  right_ = right;
  left_type_ = type_left;
  right_type_ = type_right;
  comp_op_ = comp_op;
  return RC::SUCCESS;
}

RC DefaultConditionFilter::init(Table &table, const Condition &condition)
{
  const TableMeta &table_meta = table.table_meta();
  ConDesc left;
  ConDesc right;

  AttrType type_left = UNDEFINED;
  AttrType type_right = UNDEFINED;

  if (1 == condition.left_is_attr) {
    left.is_attr = true;
    const FieldMeta *field_left = table_meta.field(condition.left_attr.attribute_name);
    if (nullptr == field_left) {
      LOG_WARN("No such field in condition. %s.%s", table.name(), condition.left_attr.attribute_name);
      return RC::SCHEMA_FIELD_MISSING;
    }
    left.attr_length = field_left->len();
    left.attr_offset = field_left->offset();

    left.value = nullptr;

    type_left = field_left->type();
  } else {
    left.is_attr = false;
    left.value = condition.left_value.data;  // 校验type 或者转换类型
    type_left = condition.left_value.type;

    if (type_left == DATES && *(int *)left.value == -1) {
      return RC::SCHEMA_FIELD_TYPE_MISMATCH;
    }

    left.attr_length = 0;
    left.attr_offset = 0;
  }

  if (1 == condition.right_is_attr) {
    right.is_attr = true;
    const FieldMeta *field_right = table_meta.field(condition.right_attr.attribute_name);
    if (nullptr == field_right) {
      LOG_WARN("No such field in condition. %s.%s", table.name(), condition.right_attr.attribute_name);
      return RC::SCHEMA_FIELD_MISSING;
    }
    right.attr_length = field_right->len();
    right.attr_offset = field_right->offset();
    type_right = field_right->type();

    right.value = nullptr;
  } else {
    right.is_attr = false;
    right.value = condition.right_value.data;
    type_right = condition.right_value.type;

    if (type_right == DATES && *(int *)right.value == -1) {
      return RC::SCHEMA_FIELD_TYPE_MISMATCH;
    }

    right.attr_length = 0;
    right.attr_offset = 0;
  }

  // 校验和转换
  if (!Stmt::check_type(type_left, type_right)) {
    return RC::SCHEMA_FIELD_TYPE_MISMATCH;
  }

  // NOTE：这里没有实现不同类型的数据比较，比如整数跟浮点数之间的对比
  // 但是选手们还是要实现。这个功能在预选赛中会出现
  return init(left, right, type_left, type_right, condition.comp);
}

bool DefaultConditionFilter::filter(const Record &rec) const
{
  char *left_value = nullptr;
  char *right_value = nullptr;
  bool left_is_null = left_type_ == AttrType::NULLS;
  bool right_is_null = right_type_ == AttrType::NULLS;

  if (left_.is_attr) {
    left_value = (char *)(rec.data() + left_.attr_offset);
    left_is_null |= left_value[left_.attr_length - 1];
  } else {
    left_value = (char *)left_.value;
  }

  if (right_.is_attr) {
    right_value = (char *)(rec.data() + right_.attr_offset);
    right_is_null |= right_value[right_.attr_length - 1];
  } else {
    right_value = (char *)right_.value;
  }

  int cmp_result = 0;

  if (left_is_null && right_is_null && comp_op_ == IS_EQUAL) {
    return true;
  } else if (!left_is_null && right_is_null && comp_op_ == IS_NOT_EQUAL) {
    return true;
  } else if (left_is_null && (comp_op_ == VALUE_EXISTS || comp_op_ == VALUE_NOT_EXISTS)) {
    return true; // 针对 comp_op_==exists，总是有left_is_null
  } else if (left_is_null || right_is_null) {
    return false;
  }

  if (left_type_ == right_type_) {
    switch (left_type_) {
      case CHARS: {
        // 字符串都是定长的，直接比较
        // 按照C字符串风格来定
        if (comp_op_ == STR_LIKE) {
          return string_like(left_value, right_value);
        } else if (comp_op_ == STR_NOT_LIKE) {
          return !string_like(left_value, right_value);
        }
        cmp_result = strcmp(left_value, right_value);
      } break;
      case INTS: {
        // 没有考虑大小端问题
        // 对int和float，要考虑字节对齐问题,有些平台下直接转换可能会跪
        int left = *(int *)left_value;
        int right = *(int *)right_value;
        cmp_result = left - right;
      } break;
      case FLOATS: {
        float left = *(float *)left_value;
        float right = *(float *)right_value;
        float result = left - right;
        cmp_result = result >= 0 ? ceil(result) : floor(result);
      } break;
      case DATES: {
        int left = *(int *)left_value;
        int right = *(int *)right_value;
        cmp_result = left - right;
      }
      default:
        break;
    }
  } else if (left_type_ == FLOATS) {
    float left = *(float *)left_value;
    float right = 0;
    switch (right_type_) {
      case CHARS: {
        right = Stmt::char_to_float(right_value);
      } break;
      case INTS: {
        right = *(int *)right_value;
      } break;
      default: {
      }
    }
    float res = left - right;
    cmp_result = res >= 0 ? ceil(res) : floor(res);
  } else if (left_type_ == INTS) {
    switch (right_type_) {
      case CHARS: {
        int left = *(int *)left_value;
        int right = Stmt::char_to_int(right_value);
        cmp_result = left - right;
      } break;
      case FLOATS: {
        float left = *(int *)left_value;
        float right = *(float *)right_value;
        float result = left - right;
        cmp_result = result >= 0 ? ceil(result) : floor(result);
      }
      default:
        break;
    }
  } else if (left_type_ == CHARS) {
    switch (right_type_) {
      case INTS: {
        int left = Stmt::char_to_int(left_value);
        int right = *(int *)right_value;
        cmp_result = left - right;
      } break;
      case FLOATS: {
        float left = Stmt::char_to_float(left_value);
        float right = *(float *)right_value;
        float result = left - right;
        cmp_result = result >= 0 ? ceil(result) : floor(result);
      } break;
      default:
        break;
    }
  }

  switch (comp_op_) {
    case EQUAL_TO:
    case IS_EQUAL:
      return cmp_result == 0;
    case LESS_EQUAL:
      return cmp_result <= 0;
    case NOT_EQUAL:
      return cmp_result != 0;
    case LESS_THAN:
      return cmp_result < 0;
    case GREAT_EQUAL:
      return cmp_result >= 0;
    case GREAT_THAN:
      return cmp_result > 0;

    default:
      break;
  }

  LOG_PANIC("Never should print this.");
  return cmp_result;  // should not go here
}

CompositeConditionFilter::~CompositeConditionFilter()
{
  if (memory_owner_) {
    delete[] filters_;
    filters_ = nullptr;
  }
}

RC CompositeConditionFilter::init(const ConditionFilter *filters[], int filter_num, bool own_memory)
{
  filters_ = filters;
  filter_num_ = filter_num;
  memory_owner_ = own_memory;
  return RC::SUCCESS;
}

RC CompositeConditionFilter::init(const ConditionFilter *filters[], int filter_num)
{
  return init(filters, filter_num, false);
}

RC CompositeConditionFilter::init(Table &table, const Condition *conditions, int condition_num)
{
  if (condition_num == 0) {
    return RC::SUCCESS;
  }
  if (conditions == nullptr) {
    return RC::INVALID_ARGUMENT;
  }

  RC rc = RC::SUCCESS;
  ConditionFilter **condition_filters = new ConditionFilter *[condition_num];
  for (int i = 0; i < condition_num; i++) {
    DefaultConditionFilter *default_condition_filter = new DefaultConditionFilter();
    rc = default_condition_filter->init(table, conditions[i]);
    if (rc != RC::SUCCESS) {
      delete default_condition_filter;
      for (int j = i - 1; j >= 0; j--) {
        delete condition_filters[j];
        condition_filters[j] = nullptr;
      }
      delete[] condition_filters;
      condition_filters = nullptr;
      return rc;
    }
    condition_filters[i] = default_condition_filter;
  }
  return init((const ConditionFilter **)condition_filters, condition_num, true);
}

bool CompositeConditionFilter::filter(const Record &rec) const
{
  for (int i = 0; i < filter_num_; i++) {
    if (!filters_[i]->filter(rec)) {
      return false;
    }
  }
  return true;
}
