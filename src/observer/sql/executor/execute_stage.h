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
// Created by Longda on 2021/4/13.
//

#ifndef __OBSERVER_SQL_EXECUTE_STAGE_H__
#define __OBSERVER_SQL_EXECUTE_STAGE_H__

#include "common/log/log.h"
#include "sql/expr/tuple_cell.h"
#include "sql/parser/parse_defs.h"
#include "sql/parser/yacc_sql.tab.h"
#include "sql/stmt/stmt.h"
#include "sql/stmt/select_stmt.h"
#include "sql/stmt/update_stmt.h"
#include "sql/stmt/delete_stmt.h"
#include "sql/stmt/insert_stmt.h"
#include "sql/stmt/filter_stmt.h"
#include "common/seda/stage.h"
#include "sql/parser/parse.h"
#include "sql/expr/tuple.h"
#include "storage/common/field_meta.h"
#include "storage/common/table.h"
#include "storage/common/field.h"
#include "storage/common/condition_filter.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <algorithm>
#include "rc.h"

class SQLStageEvent;
class SessionEvent;
class SelectStmt;
class Pretable;

class ExecuteStage : public common::Stage {
public:
  ~ExecuteStage();
  static Stage *make_stage(const std::string &tag);
  static RC select_to_pretable(
      SelectStmt *select_stmt, Pretable *&&res, bool build_exists_results, FieldExpr *exists_left);
  static Pretable *Selects_to_pretable(
      Db *db, Value *value, std::unordered_set<Table *> &tables, bool build_exists_results, FieldExpr *exists_left);

protected:
  // common function
  ExecuteStage(const char *tag);
  bool set_properties() override;

  bool initialize() override;
  void cleanup() override;
  void handle_event(common::StageEvent *event) override;
  void callback_event(common::StageEvent *event, common::CallbackContext *context) override;

  void handle_request(common::StageEvent *event);
  RC do_help(SQLStageEvent *session_event);
  RC do_create_table(SQLStageEvent *sql_event);
  RC do_create_index(SQLStageEvent *sql_event);
  RC do_show_index(SQLStageEvent *sql_event);
  RC do_show_tables(SQLStageEvent *sql_event);
  RC do_desc_table(SQLStageEvent *sql_event);
  RC do_select(SQLStageEvent *sql_event);
  RC do_insert(SQLStageEvent *sql_event);
  RC do_delete(SQLStageEvent *sql_event);
  RC do_update(SQLStageEvent *sql_event);
  RC do_begin(SQLStageEvent *sql_event);
  RC do_commit(SQLStageEvent *sql_event);
  RC do_clog_sync(SQLStageEvent *sql_event);
  RC do_drop_table(SQLStageEvent *sql_event);
  RC do_update_table(SQLStageEvent *sql_event);
  RC value_check(const int &value_num, const Value *values) const;
  void print_fields(std::stringstream &ss, const std::vector<Field> &fields, bool multi, int num);
  RC compute_value_from_select(Db *db, Value *value);
  RC check_updates(Db *db, Updates &updates);

protected:
private:
  Stage *default_storage_stage_ = nullptr;
  Stage *mem_storage_stage_ = nullptr;
};

class TupleSet {
 public:
  TupleSet() = default;
  int table_num();
  TupleSet(const Tuple *t, Table *table);
  TupleSet(const TupleSet *t);
  TupleSet *copy() const;

  TupleSet *generate_combine(const TupleSet *t2);
  void filter_fields(const std::vector<int> &orders);
  const std::vector<TupleCell> &cells() const;

  void push(const TupleCell &cell); // only for in values
  TupleCell &get_cell(int idx);

  const std::string& data() const { return data_; }

  int size() const { return data_.size(); }
  bool in(const TupleCell &cell) const;
  bool not_in(const TupleCell &cell) const;
  bool equal(const TupleCell &cell, int offset) const;

  // void combine(const TupleSet *t2);
  // void push(const Field &p, const TupleCell &cell);
  // const Field &get_field(int idx);
  // const Field *get_field(const char *table_name, const char *field_name) const;
  // int index(const Field &field) const;
  // int index(const Table* table, const FieldMeta& field_meta) const;
  // int index_with_aggr(const Field &field) const;
  // int get_offset(const char *table_name, const char *field_name) const;

 private:
  // int table_num_ = 0;
  // std::vector<std::pair<Table*, FieldMeta>> metas_;
  // std::vector<std::pair<Table*, Field>> metas_;
  // each meta has new fieldmeta
  std::vector<TupleCell> cells_;
  std::string data_;
};

class Pretable {
 public:
  Pretable() = default;
  Pretable(const Pretable &pretable);
  Pretable(Pretable&& t);
  Pretable& operator=(Pretable&& t);
  Pretable(ValueList* valuelist);     // only for id in (1,2,3);
  // ~Pretable() = default;

  RC init(Table *table, FilterStmt *old_filter);
  RC exists(Pretable* pre2, FilterStmt* filter);
  RC join(Pretable *pre2, FilterStmt *filter);
  void print(std::stringstream &os, int num);
  void filter_fields(const std::vector<Field> &fields);
  RC aggregate(const std::vector<Field> fields);
  RC aggregate_max(int idx, TupleCell *res, int group_id);
  RC aggregate_sum(int idx, TupleCell *res, int group_id);
  RC aggregate_min(int idx, TupleCell *res, int group_id);
  RC aggregate_avg(int idx, TupleCell *res, int group_id);
  RC aggregate_count(int idx, TupleCell *res, int group_id);
  void order_by(const std::vector<OrderByField> &fields);

  CompositeConditionFilter *make_cond_filter(std::vector<FilterUnit*> &units);
  CompositeConditionFilter *make_having_filter(Condition *conditions, int num);
  ConDesc make_cond_desc(Expression *expr);
  int get_offset(const char *table_name, const char *field_name) const;

  int index(const Field &field) const;
  int index(const char *table_name, const char *field_name) const;

  const FieldMeta *field_meta(int idx) const { return fields_[idx].metac(); }
  const FieldMeta *field_meta(const Field &field) const;
  const FieldMeta *field_meta(const char *table_name, const char *field_name) const;

  const Field *field(const char *table_name, const char *field_name) const;
  const Field *field(const Field &field) const;
  const Field *field(int idx) const { return &fields_[idx]; }

  const Field *field_with_aggr(const char *table_name, const char *field_name, AggreType type) const;

  // std::vector<TupleSet>::iterator begin() { return tuples_.begin(); }
  // std::vector<TupleSet>::iterator end() { return tuples_.end(); }

  RC assign_row_to_value(Value *value);

  bool in(Value *value) const;
  bool in(TupleCell &cell) const;
  bool not_in(TupleCell &cell) const;
  bool exists(TupleCell &cell) const;
  bool not_exists(TupleCell &cell) const;
  // int group_num() { return get_group(0).size(); }
  std::vector<TupleSet> &get_group(int group_id) { return groups_[group_id]; }
  int group_num() const { return groups_.size(); }
  int tuple_num() const;
  void groupby(const std::vector<Field> groupby_fields);
  void having(Condition *having_conditions, int having_condition_num);
  bool only_one_cell() const {
    return groups_.size() == 1
        && groups_[0].size() == 1
        && groups_[0][0].cells().size() == 1; }
  bool valid_operation(CompOp op) const;
  TupleCell get_first_cell() { return groups_[0][0].get_cell(0); }

public:
  std::vector<TupleCell> exists_results; // exists 子句预先计算结果，未去重（因为未定义TupleCell上的hash）！！

private:
  std::vector<std::vector<TupleSet>> groups_;
  std::vector<Field> fields_;
  std::vector<Table*> tables_;

};

class PretableHash {
 public:

  PretableHash(AttrType type);
  int get_value(const TupleCell &cell);

  void *map_ = nullptr;
  AttrType type_;
  int index_;
  int null_index_;
};

#endif  //__OBSERVER_SQL_EXECUTE_STAGE_H__
