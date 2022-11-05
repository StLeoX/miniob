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
// Created by Meiyi & Wangyunlai.wyl on 2021/5/18.
//

#include "storage/common/index_meta.h"
#include "storage/common/field_meta.h"
#include "storage/common/table_meta.h"
#include "common/lang/string.h"
#include "common/log/log.h"
#include "rc.h"
#include "json/json.h"
#include "json/value.h"
#include <string>
#include <vector>

const static Json::StaticString FIELD_NAME("name");
const static Json::StaticString FIELD_FIELD_NAMES("field_names");
const static Json::StaticString FIELD_UNIQUE("unique");

RC IndexMeta::init(const char *name, const std::vector<FieldMeta> &fields, bool unique)
{
  if (common::is_blank(name)) {
    LOG_ERROR("Failed to init index, name is empty.");
    return RC::INVALID_ARGUMENT;
  }
  for (const FieldMeta &field : fields) {
    fields_.push_back(field.name());
  }
  name_ = name;
  unique_ = unique;
  return RC::SUCCESS;
}

void IndexMeta::to_json(Json::Value &json_value) const
{
  json_value[FIELD_NAME] = name_;
  std::string tp;
  for (auto &field : fields_) {
    tp += ";"+field;
  }
  json_value[FIELD_FIELD_NAMES] = tp;
  json_value[FIELD_UNIQUE] = unique_;
}

RC IndexMeta::from_json(const TableMeta &table, const Json::Value &json_value, IndexMeta &index)
{
  const Json::Value &name_value = json_value[FIELD_NAME];
  const Json::Value &fields_value = json_value[FIELD_FIELD_NAMES];
  const Json::Value &unique_value = json_value[FIELD_UNIQUE];
  if (!name_value.isString()) {
    LOG_ERROR("Index name is not a string. json value=%s", name_value.toStyledString().c_str());
    return RC::GENERIC_ERROR;
  }

  if (!fields_value.isString()) {
    LOG_ERROR("Field name of index [%s] is not a string. json value=%s",
        name_value.asCString(),
        fields_value.toStyledString().c_str());
    return RC::GENERIC_ERROR;
  }

  if (!unique_value.isBool()) {
    LOG_ERROR("Unique is not a bool. json value=%s", unique_value.toStyledString().c_str());
    return RC::GENERIC_ERROR;
  }

  std::vector<std::string> field_names;
  std::string s = fields_value.asString();
  // https://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c
  std::string delimiter(1, ';');
  size_t pos = 0;
  std::string token;
  while ((pos = s.find(delimiter)) != std::string::npos) {
    field_names.push_back(s.substr(0, pos));
    s.erase(0, pos + delimiter.length());
  }
  field_names.push_back(s);
  std::vector<FieldMeta> fields;

  for (auto &field : field_names) {
    if (field.empty()) {
      continue;
    }
    const FieldMeta *meta = table.field(field.c_str());
    if (nullptr == meta) {
      LOG_ERROR("Deserialize index [%s]: no such field: %s", name_value.asCString(), field.c_str());
      return RC::SCHEMA_FIELD_MISSING;
    }
    fields.push_back(*meta);
  }

  return index.init(name_value.asCString(), fields, unique_value.asBool());
}

const char *IndexMeta::name() const
{
  return name_.c_str();
}

void IndexMeta::desc(std::ostream &os) const
{
  os << "index name=" << name_;
  for (auto &field : fields_) {
    os << ", field=" << field;
  }
}
