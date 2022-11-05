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
// Created by Meiyi & Wangyunlai on 2021/5/12.
//

#ifndef __OBSERVER_STORAGE_COMMON_INDEX_META_H__
#define __OBSERVER_STORAGE_COMMON_INDEX_META_H__

#include <string>
#include <vector>
#include <stdexcept>
#include "rc.h"

class TableMeta;
class FieldMeta;

namespace Json {
class Value;
}  // namespace Json

class IndexMeta {
public:
  IndexMeta() = default;

  RC init(const char *name, const std::vector<FieldMeta> &fields, bool unique);

public:
  const char *name() const;
  const std::vector<std::string> &fields() const { return fields_; }
  const std::string &field(int i) const {
    if (i < 0 || (size_t)i >= fields_.size()) {
      throw std::invalid_argument("indexmeta: field out of range");
    }
    return fields_[i];
  }

  std::string fields_combine_name() const  {
    std::string tp = fields_[0];
    for (size_t i = 1; i < fields_.size(); i++) {
      tp += ", " + fields_[i];
    }
    return tp;
  }
  int field_num() { return fields_.size(); }
  // const char *field() const;
  bool is_unique() const { return unique_; }
  void set_unique(bool f) { unique_ = f; }

  void desc(std::ostream &os) const;

public:
  void to_json(Json::Value &json_value) const;
  static RC from_json(const TableMeta &table, const Json::Value &json_value, IndexMeta &index);

protected:
  bool unique_;
  std::string name_;   // index's name
  std::vector<std::string> fields_;  // field's name
};
#endif  // __OBSERVER_STORAGE_COMMON_INDEX_META_H__
