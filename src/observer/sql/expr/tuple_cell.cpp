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
// Created by WangYunlai on 2022/07/05.
//

#include "sql/expr/tuple_cell.h"
#include "sql/parser/parse_defs.h"
#include "sql/stmt/stmt.h"
#include "storage/common/field.h"
#include "common/log/log.h"
#include "util/comparator.h"
#include "sql/parser/date.cpp"
#include "util/util.h"

void TupleCell::to_string(std::ostream &os) const
{
  switch (attr_type_) {
  case INTS: {
    os << *(int *)data_;
  } break;
  case FLOATS: {
    float v = *(float *)data_;
    os << double2string(v);
  } break;
  case TEXTS: // 复用
  case CHARS: {
    for (int i = 0; i < length_; i++) {
      if (data_[i] == '\0') {
        break;
      }
      os << data_[i];
    }
  } break;
  case DATES: {
    os << date_to_string(*(int *)data_);
  } break;
  case NULLS: {
    os << "null";
  } break;
  default: {
    LOG_WARN("unsupported attr type: %d", attr_type_);
  } break;
  }
}

int TupleCell::compare(const TupleCell &other) const
{
  if (this->attr_type_ == other.attr_type_) {
    switch (this->attr_type_) {
      case DATES: return compare_date(this->data_, other.data_);
    case INTS: return compare_int(this->data_, other.data_);
    case FLOATS: return compare_float(this->data_, other.data_);
    case CHARS: return compare_string(this->data_, this->length_, other.data_, other.length_);
    case NULLS: return 0;
    default: {
      LOG_WARN("unsupported type: %d", this->attr_type_);
    }
    }
  } else if (this->attr_type_ == INTS && other.attr_type_ == FLOATS) {
    float this_data = *(int *)data_;
    return compare_float(&this_data, other.data_);
  } else if (this->attr_type_ == FLOATS && other.attr_type_ == INTS) {
    float other_data = *(int *)other.data_;
    return compare_float(data_, &other_data);
  } else if (this->attr_type_ == CHARS && other.attr_type_ == INTS) {
    float left = Stmt::char_to_float(data_);
    float right = *(int *)other.data_;
    return compare_int(&left, &right);
  } else if (this->attr_type_ == INTS && other.attr_type_ == CHARS) {
    float left = *(int *)data_;
    float right = Stmt::char_to_float(other.data_);
    return compare_float(&left, &right);
  } else if (this->attr_type_ == FLOATS && other.attr_type_ == CHARS) {
    float ans = Stmt::char_to_float(other.data_);
    return compare_float(data_, &ans);
  } else if (this->attr_type_ == CHARS && other.attr_type_ == FLOATS) {
    float ans = Stmt::char_to_float(data_);
    return compare_float(&ans, other.data_);
  } else if (this->attr_type_ == NULLS) {
    return -1;
  } else if (other.attr_type() == NULLS) {
    return 1;
  }
  LOG_ERROR("unimplement");
  return -1;
}
