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
// Created by Meiyi & Longda on 2021/4/13.
//

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "execute_stage.h"

#include "common/io/io.h"
#include "common/log/log.h"
#include "common/lang/defer.h"
#include "common/seda/timer_stage.h"
#include "common/lang/string.h"
#include "rc.h"
#include "session/session.h"
#include "event/storage_event.h"
#include "event/sql_event.h"
#include "event/session_event.h"
#include "sql/expr/expression.h"
#include "sql/expr/tuple.h"
#include "sql/expr/tuple_cell.h"
#include "sql/operator/table_scan_operator.h"
#include "sql/operator/index_scan_operator.h"
#include "sql/operator/predicate_operator.h"
#include "sql/operator/delete_operator.h"
#include "sql/operator/project_operator.h"
#include "sql/parser/parse_defs.h"
#include "sql/stmt/filter_stmt.h"
#include "sql/stmt/select_stmt.h"
#include "sql/stmt/stmt.h"
#include "sql/stmt/update_stmt.h"
#include "storage/common/field.h"
#include "storage/common/field_meta.h"
#include "storage/common/table.h"
#include "storage/common/table_meta.h"
#include "storage/index/bplus_tree.h"
#include "storage/index/index.h"
#include "storage/default/default_handler.h"
#include "storage/common/condition_filter.h"
#include "storage/record/record.h"
#include "storage/trx/trx.h"
#include "storage/clog/clog.h"

using namespace common;

//RC create_selection_executor(
//   Trx *trx, const Selects &selects, const char *db, const char *table_name, SelectExeNode &select_node);

//! Constructor
ExecuteStage::ExecuteStage(const char *tag) : Stage(tag)
{}

//! Destructor
ExecuteStage::~ExecuteStage()
{}

//! Parse properties, instantiate a stage object
Stage *ExecuteStage::make_stage(const std::string &tag)
{
  ExecuteStage *stage = new (std::nothrow) ExecuteStage(tag.c_str());
  if (stage == nullptr) {
    LOG_ERROR("new ExecuteStage failed");
    return nullptr;
  }
  stage->set_properties();
  return stage;
}

//! Set properties for this object set in stage specific properties
bool ExecuteStage::set_properties()
{
  //  std::string stageNameStr(stageName);
  //  std::map<std::string, std::string> section = theGlobalProperties()->get(
  //    stageNameStr);
  //
  //  std::map<std::string, std::string>::iterator it;
  //
  //  std::string key;

  return true;
}

//! Initialize stage params and validate outputs
bool ExecuteStage::initialize()
{
  LOG_TRACE("Enter");

  std::list<Stage *>::iterator stgp = next_stage_list_.begin();
  default_storage_stage_ = *(stgp++);
  mem_storage_stage_ = *(stgp++);

  LOG_TRACE("Exit");
  return true;
}

//! Cleanup after disconnection
void ExecuteStage::cleanup()
{
  LOG_TRACE("Enter");

  LOG_TRACE("Exit");
}

void ExecuteStage::handle_event(StageEvent *event)
{
  LOG_TRACE("Enter\n");

  handle_request(event);

  LOG_TRACE("Exit\n");
  return;
}

void ExecuteStage::callback_event(StageEvent *event, CallbackContext *context)
{
  LOG_TRACE("Enter\n");

  // here finish read all data from disk or network, but do nothing here.

  LOG_TRACE("Exit\n");
  return;
}

void ExecuteStage::handle_request(common::StageEvent *event)
{
  SQLStageEvent *sql_event = static_cast<SQLStageEvent *>(event);
  SessionEvent *session_event = sql_event->session_event();
  Stmt *stmt = sql_event->stmt();
  Session *session = session_event->session();
  Query *sql = sql_event->query();

  if (stmt != nullptr) {
    switch (stmt->type()) {
    case StmtType::SELECT: {
      do_select(sql_event);
    } break;
    case StmtType::INSERT: {
      do_insert(sql_event);
    } break;
    case StmtType::UPDATE: {
      do_update(sql_event);
    } break;
    case StmtType::DELETE: {
      do_delete(sql_event);
    } break;
    default: {
      LOG_WARN("should not happen. please implenment");
    } break;
    }
  } else {
    switch (sql->flag) {
    case SCF_HELP: {
      do_help(sql_event);
    } break;
    case SCF_CREATE_TABLE: {
      do_create_table(sql_event);
    } break;
    case SCF_CREATE_INDEX: {
      do_create_index(sql_event);
    } break;
    case SCF_SHOW_INDEX: {
      do_show_index(sql_event);
    } break;
    case SCF_SHOW_TABLES: {
      do_show_tables(sql_event);
    } break;
    case SCF_DESC_TABLE: {
      do_desc_table(sql_event);
    } break;
    case SCF_UPDATE: {
      do_update_table(sql_event);
    } break;
    case SCF_DROP_TABLE: {
      do_drop_table(sql_event);
    } break;
    case SCF_DROP_INDEX:
    case SCF_LOAD_DATA: {
      default_storage_stage_->handle_event(event);
    } break;
    case SCF_SYNC: {
      /*
      RC rc = DefaultHandler::get_default().sync();
      session_event->set_response(strrc(rc));
      */
    } break;
    case SCF_BEGIN: {
      do_begin(sql_event);
      /*
      session_event->set_response("SUCCESS\n");
      */
    } break;
    case SCF_COMMIT: {
      do_commit(sql_event);
      /*
      Trx *trx = session->current_trx();
      RC rc = trx->commit();
      session->set_trx_multi_operation_mode(false);
      session_event->set_response(strrc(rc));
      */
    } break;
    case SCF_CLOG_SYNC: {
      do_clog_sync(sql_event);
    }
    case SCF_ROLLBACK: {
      Trx *trx = session_event->get_client()->session->current_trx();
      RC rc = trx->rollback();
      session->set_trx_multi_operation_mode(false);
      session_event->set_response(strrc(rc));
    } break;
    case SCF_EXIT: {
      // do nothing
      const char *response = "Unsupported\n";
      session_event->set_response(response);
    } break;
    default: {
      LOG_ERROR("Unsupported command=%d\n", sql->flag);
    }
    }
  }
}

void end_trx_if_need(Session *session, Trx *trx, bool all_right)
{
  if (!session->is_trx_multi_operation_mode()) {
    if (all_right) {
      trx->commit();
    } else {
      trx->rollback();
    }
  }
}

void print_tuple_header(std::ostream &os, const ProjectOperator &oper)
{
  const int cell_num = oper.tuple_cell_num();
  const TupleCellSpec *cell_spec = nullptr;
  for (int i = 0; i < cell_num; i++) {
    oper.tuple_cell_spec_at(i, cell_spec);
    if (i != 0) {
      os << " | ";
    }

    if (cell_spec->alias()) {
      os << cell_spec->alias();
    }
  }

  if (cell_num > 0) {
    os << '\n';
  }
}

void tuple_to_string(std::ostream &os, const Tuple &tuple)
{
  TupleCell cell;
  RC rc = RC::SUCCESS;
  bool first_field = true;
  for (int i = 0; i < tuple.cell_num(); i++) {
    rc = tuple.cell_at(i, cell);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to fetch field of cell. index=%d, rc=%s", i, strrc(rc));
      break;
    }

    if (!first_field) {
      os << " | ";
    } else {
      first_field = false;
    }
    cell.to_string(os);
  }
}

IndexScanOperator *try_to_create_index_scan_operator(FilterStmt *filter_stmt)
{
  const std::vector<FilterUnit *> &filter_units = filter_stmt->filter_units();
  if (filter_units.empty() ) {
    return nullptr;
  }

  // 在所有过滤条件中，找到字段与值做比较的条件，然后判断字段是否可以使用索引
  // 如果是多列索引，这里的处理需要更复杂。
  // 这里的查找规则是比较简单的，就是尽量找到使用相等比较的索引
  // 如果没有就找范围比较的，但是直接排除不等比较的索引查询. (你知道为什么?)
  const FilterUnit *better_filter = nullptr;
  for (const FilterUnit * filter_unit : filter_units) {
    if (filter_unit->comp() == NOT_EQUAL) {
      continue;
    }

    Expression *left = filter_unit->left();
    Expression *right = filter_unit->right();
    if (left->type() == ExprType::FIELD && right->type() == ExprType::VALUE) {
    } else if (left->type() == ExprType::VALUE && right->type() == ExprType::FIELD) {
      std::swap(left, right);
    } else{
      continue;
    }
    FieldExpr &left_field_expr = *(FieldExpr *)left;
    const Field &field = left_field_expr.field();
    const Table *table = field.table();
    Index *index = table->find_index_by_field(field.field_name());
    if (index != nullptr) {
      if (better_filter == nullptr) {
        better_filter = filter_unit;
      } else if (filter_unit->comp() == EQUAL_TO) {
        better_filter = filter_unit;
    	break;
      }
    }
  }

  if (better_filter == nullptr) {
    return nullptr;
  }

  Expression *left = better_filter->left();
  Expression *right = better_filter->right();
  CompOp comp = better_filter->comp();
  if (left->type() == ExprType::VALUE && right->type() == ExprType::FIELD) {
    std::swap(left, right);
    switch (comp) {
    case EQUAL_TO:    { comp = EQUAL_TO; }    break;
    case LESS_EQUAL:  { comp = GREAT_THAN; }  break;
    case NOT_EQUAL:   { comp = NOT_EQUAL; }   break;
    case LESS_THAN:   { comp = GREAT_EQUAL; } break;
    case GREAT_EQUAL: { comp = LESS_THAN; }   break;
    case GREAT_THAN:  { comp = LESS_EQUAL; }  break;
    default: {
    	LOG_WARN("should not happen");
    }
    }
  }


  FieldExpr &left_field_expr = *(FieldExpr *)left;
  const Field &field = left_field_expr.field();
  const Table *table = field.table();
  Index *index = table->find_index_by_field(field.field_name());
  assert(index != nullptr);

  ValueExpr &right_value_expr = *(ValueExpr *)right;
  TupleCell value;
  right_value_expr.get_tuple_cell(value);

  const TupleCell *left_cell = nullptr;
  const TupleCell *right_cell = nullptr;
  bool left_inclusive = false;
  bool right_inclusive = false;

  switch (comp) {
  case EQUAL_TO: {
    left_cell = &value;
    right_cell = &value;
    left_inclusive = true;
    right_inclusive = true;
  } break;

  case LESS_EQUAL: {
    left_cell = nullptr;
    left_inclusive = false;
    right_cell = &value;
    right_inclusive = true;
  } break;

  case LESS_THAN: {
    left_cell = nullptr;
    left_inclusive = false;
    right_cell = &value;
    right_inclusive = false;
  } break;

  case GREAT_EQUAL: {
    left_cell = &value;
    left_inclusive = true;
    right_cell = nullptr;
    right_inclusive = false;
  } break;

  case GREAT_THAN: {
    left_cell = &value;
    left_inclusive = false;
    right_cell = nullptr;
    right_inclusive = false;
  } break;

  default: {
    LOG_WARN("should not happen. comp=%d", comp);
  } break;
  }

  IndexScanOperator *oper = new IndexScanOperator(table, index,
       left_cell, left_inclusive, right_cell, right_inclusive);

  LOG_INFO("use index for scan: %s in table %s", index->index_meta().name(), table->name());
  return oper;
}

void reorder_fields(std::vector<Field> &fields)
{
  if (fields[0].aggr_type() != A_NO) {
    std::vector<Field> tp;
    for (int i = fields.size()-1; i >= 0; i--) {
      tp.push_back(fields[i]);
    }
    fields.swap(tp);
    return;
  }
}

RC ExecuteStage::select_to_pretable(
    SelectStmt *select_stmt, Pretable *&&res, bool build_exists_results, FieldExpr *exists_left)
{
  FilterStmt *filter_stmt = select_stmt->filter_stmt();

  std::vector<Pretable*> pretables;

  RC rc = RC::SUCCESS;

  for (size_t i = 0; i < select_stmt->tables().size(); i++) {
    Pretable *pre = new Pretable();
    rc = pre->init(select_stmt->tables()[i], filter_stmt);
    if (rc != RC::RECORD_EOF) {
      delete pre;
      for (auto &t : pretables)
        delete t;
      return rc;
    }
    pretables.push_back(pre);
  }

  // no relevant field or something
  if (pretables.empty()) {
    LOG_ERROR("No table or No relevant condition");
    rc = RC::INTERNAL;
    return rc;
  }

  res = pretables[0];
  // build exists results
  if(build_exists_results) {
    auto right_table = select_stmt->tables()[0];
    auto left_table = select_stmt->tables()[1];
    int left_field_idx = left_table->table_meta().find_idx_by_name(exists_left->field_name());
    assert( left_field_idx != -1);
    int right_field_idx = -1;
    int hit_left_or_right = -1;

    Operator *left_scan_oper = new TableScanOperator(left_table);
    PredicateOperator left_po(new FilterStmt()); // nothing to filter
    left_po.add_child(left_scan_oper);
    left_po.open();

    rc = RC::SUCCESS;
    // 开始外循环
    while((rc = left_po.next()) == RC::SUCCESS){
      Tuple *left = left_po.current_tuple();
      assert(left);
      TupleCell cell;
      assert(left->cell_at(left_field_idx, cell) == RC::SUCCESS);
      ValueExpr *left_determined = new ValueExpr(cell);
      // FilterStmt* right_filter_determined = new FilterStmt();
      // 将 filter 的unit0确定
      auto unit0 = filter_stmt->filter_units()[0];
      // 检测hit情况，是全局的，类似 right_field_idx；-1 未初始化，0 left，1 right
      if(hit_left_or_right == -1) {
        LOG_DEBUG("exist: %s\n", exists_left->to_string().c_str());
        LOG_DEBUG("left: %s\n",((FieldExpr*)(unit0->left()))->to_string().c_str());
        LOG_DEBUG("right: %s\n",((FieldExpr*)(unit0->right()))->to_string().c_str());
        if (*(FieldExpr *)(unit0->left()) == *exists_left)
          hit_left_or_right = 0;
        else if (*(FieldExpr *)(unit0->right()) == *exists_left)
          hit_left_or_right = 1;
        else {
          LOG_ERROR("error exist field\n");
          return RC::INTERNAL;
        }
        // 设置right_field_idx
        if(right_field_idx == -1) {
          if(hit_left_or_right == 0)
            right_field_idx = right_table->table_meta().find_idx_by_name(((FieldExpr*)unit0->right())->field_name());
          else if(hit_left_or_right == 1)
            right_field_idx = right_table->table_meta().find_idx_by_name(((FieldExpr*)unit0->left())->field_name());
          else
            return RC::INTERNAL;
        }
        assert(right_field_idx != -1);
      }
      if(hit_left_or_right == 0)
        unit0->set_left(left_determined);
      else if(hit_left_or_right == 1)
        unit0->set_right(left_determined);
      else
        return RC::INTERNAL;

      // 开始内循环
      Operator *right_scan_oper = new TableScanOperator(right_table);
      PredicateOperator right_po(filter_stmt);
      right_po.add_child(right_scan_oper);
      right_po.open();
      while((rc = right_po.next()) == RC::SUCCESS) {
        Tuple *right = right_po.current_tuple();
        assert(right);
        TupleCell cell_right;
        right->cell_at(right_field_idx, cell_right);
        // 此处实际上不需要求出cell_right，因为我们是记录“左表”相应属性的存在性。
        res->exists_results.push_back(cell);
      }
    }
  }

  auto iter = std::next(pretables.begin());
  while (iter != pretables.end()) {
    rc = res->join(*iter, filter_stmt);
    delete *iter;
    iter = pretables.erase(iter);
    if (rc != RC::SUCCESS) {
      while (iter != pretables.end()) {
        delete *iter;
        iter = pretables.erase(iter);
      }
      LOG_ERROR("join fails");
      return rc;
    }
  }

  rc = RC::SUCCESS;
  // group by
  res->groupby(select_stmt->groupby_fields());

  // aggregate
  if (select_stmt->aggregate_num() > 0) {
    rc = res->aggregate(select_stmt->query_fields());
    // having
    res->having(select_stmt->having_conditions(), select_stmt->having_condition_num());
  } else {
    // order by fields, if necessary
    res->order_by(select_stmt->order_by_fields());
    res->filter_fields(select_stmt->query_fields());
  }
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to aggregate");
    return rc;
  }

  if (rc != RC::SUCCESS) {
    LOG_ERROR("failed to exist");
    return rc;
  }

  return rc;
}

RC ExecuteStage::do_select(SQLStageEvent *sql_event)
{
  SelectStmt *select_stmt = (SelectStmt *)(sql_event->stmt());
  SessionEvent *session_event = sql_event->session_event();
  // FilterStmt *filter_stmt = select_stmt->filter_stmt();
  // reorder_fields(select_stmt->query_fields());

  Pretable *pretable = new Pretable();
  RC rc = select_to_pretable(select_stmt, std::move(pretable), false, nullptr);
  if (rc!=RC::SUCCESS) {
    session_event->set_response("FAILURE\n");
    return rc;
  }
  std::stringstream ss;

  print_fields(ss, select_stmt->query_fields(), select_stmt->tables().size() > 1, select_stmt->query_num());
  pretable->print(ss, select_stmt->query_num());

  session_event->set_response(ss.str());
  return rc;
}

/*
RC ExecuteStage::do__select(SQLStageEvent *sql_event)
{
  SelectStmt *select_stmt = (SelectStmt *)(sql_event->stmt());
  SessionEvent *session_event = sql_event->session_event();
  RC rc = RC::SUCCESS;
  if (select_stmt->tables().size() != 1) {
    LOG_WARN("select more than 1 tables is not supported");
    rc = RC::UNIMPLENMENT;
    return rc;
  }

  Operator *scan_oper = try_to_create_index_scan_operator(select_stmt->filter_stmt());
  if (nullptr == scan_oper) {
    scan_oper = new TableScanOperator(select_stmt->tables()[0]);
  }

  DEFER([&] () {delete scan_oper;});

  PredicateOperator pred_oper(select_stmt->filter_stmt());
  pred_oper.add_child(scan_oper);
  ProjectOperator project_oper;
  project_oper.add_child(&pred_oper);
  for (const Field &field : select_stmt->query_fields()) {
    project_oper.add_projection(field.table(), field.metac());
  }
  rc = project_oper.open();
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to open operator");
    return rc;
  }

  std::stringstream ss;
  print_tuple_header(ss, project_oper);
  while ((rc = project_oper.next()) == RC::SUCCESS) {
    // get current record
    // write to response
    Tuple * tuple = project_oper.current_tuple();
    if (nullptr == tuple) {
      rc = RC::INTERNAL;
      LOG_WARN("failed to get current record. rc=%s", strrc(rc));
      break;
    }

    tuple_to_string(ss, *tuple);
    ss << std::endl;
  }

  if (rc != RC::RECORD_EOF) {
    LOG_WARN("something wrong while iterate operator. rc=%s", strrc(rc));
    project_oper.close();
  } else {
    rc = project_oper.close();
  }
  session_event->set_response(ss.str());
  return rc;
}
*/

RC ExecuteStage::do_help(SQLStageEvent *sql_event)
{
  SessionEvent *session_event = sql_event->session_event();
  const char *response = "show tables;\n"
                         "desc `table name`;\n"
                         "create table `table name` (`column name` `column type`, ...);\n"
                         "create index `index name` on `table` (`column`);\n"
                         "insert into `table` values(`value1`,`value2`);\n"
                         "update `table` set column=value [where `column`=`value`];\n"
                         "delete from `table` [where `column`=`value`];\n"
                         "select [ * | `columns` ] from `table`;\n";
  session_event->set_response(response);
  return RC::SUCCESS;
}

RC ExecuteStage::do_create_table(SQLStageEvent *sql_event)
{
  CreateTable &create_table = sql_event->query()->sstr.create_table;
  SessionEvent *session_event = sql_event->session_event();
  Db* db = session_event->session()->get_current_db();
  RC rc = db->create_table(create_table.relation_name,
			create_table.attribute_count, create_table.attributes);
  if (rc == RC::SUCCESS) {
    session_event->set_response("SUCCESS\n");
  } else {
    session_event->set_response("FAILURE\n");
  }
  return rc;
}

RC ExecuteStage::do_drop_table(SQLStageEvent *sql_event)
{
  const DropTable &drop_table = sql_event->query()->sstr.drop_table;
  SessionEvent *session_event = sql_event->session_event();
  Db *db = session_event->session()->get_current_db();
  RC rc = db->drop_table(drop_table.relation_name);
  if (rc == RC::SUCCESS) {
    session_event->set_response("SUCCESS\n");
  } else {
    session_event->set_response("FAILURE\n");
  }
  return rc;
}

RC ExecuteStage::do_create_index(SQLStageEvent *sql_event)
{
  SessionEvent *session_event = sql_event->session_event();
  Db *db = session_event->session()->get_current_db();
  const CreateIndex &create_index = sql_event->query()->sstr.create_index;
  Table *table = db->find_table(create_index.relation_name);
  if (nullptr == table) {
    session_event->set_response("FAILURE\n");
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }

  RC rc = table->create_index(nullptr, create_index.index_name, create_index.attribute_names, create_index.attribute_num, create_index.unique);
  sql_event->session_event()->set_response(rc == RC::SUCCESS ? "SUCCESS\n" : "FAILURE\n");
  return rc;
}

RC ExecuteStage::do_show_index(SQLStageEvent *sql_event)
{
  SessionEvent *session_event = sql_event->session_event();
  Db *db = session_event->session()->get_current_db();
  const ShowIndex &show_index = sql_event->query()->sstr.show_index;
  Table *table = db->find_table(show_index.relation_name);
  if (nullptr == table) {
    session_event->set_response("FAILURE\n");
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }

  std::stringstream ss;
  table->show_index(ss);
  session_event->set_response(ss.str());
  return RC::SUCCESS;
}

RC ExecuteStage::do_show_tables(SQLStageEvent *sql_event)
{
  SessionEvent *session_event = sql_event->session_event();
  Db *db = session_event->session()->get_current_db();
  std::vector<std::string> all_tables;
  db->all_tables(all_tables);
  if (all_tables.empty()) {
    session_event->set_response("No table\n");
  } else {
    std::stringstream ss;
    for (const auto &table : all_tables) {
      ss << table << std::endl;
    }
    session_event->set_response(ss.str().c_str());
  }
  return RC::SUCCESS;
}

RC ExecuteStage::do_desc_table(SQLStageEvent *sql_event)
{
  Query *query = sql_event->query();
  Db *db = sql_event->session_event()->session()->get_current_db();
  const char *table_name = query->sstr.desc_table.relation_name;
  Table *table = db->find_table(table_name);
  std::stringstream ss;
  if (table != nullptr) {
    table->table_meta().desc(ss);
  } else {
    ss << "No such table: " << table_name << std::endl;
  }
  sql_event->session_event()->set_response(ss.str().c_str());
  return RC::SUCCESS;
}

bool check_attr_in_table(Table *table, const RelAttr &attr)
{
  if (attr.relation_name != nullptr && strcmp(table->name(), attr.relation_name) != 0) {
    return false;
  }
  return table->table_meta().field(attr.attribute_name) != nullptr;
}

// type conversion is in compare
RC ExecuteStage::check_updates(Db *db, Updates &updates)
{
  const char *table_name = updates.relation_name;
  Table *table = db->find_table(table_name);
  const TableMeta &table_meta = table->table_meta();
  if (table == nullptr) {
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }
  RC rc = RC::SUCCESS;
  for (int i = 0; i < updates.attribute_num; i++) {
    const char *attr_name = updates.attributes[i];
    Value *value = &updates.values[i];
    if (value->type == DATES && *(int *)value->data == -1) {
      return RC::RECORD_INVALID_KEY;
    }

    const FieldMeta *fmeta = table_meta.field(attr_name);
    if (fmeta == nullptr) {
      return RC::SCHEMA_FIELD_MISSING;
    }

    if (value->type == SELECTS) {
      rc = compute_value_from_select(db, value);
      if (rc != RC::SUCCESS) {
        return rc;
      }
    }

    if (value->type == NULLS && !fmeta->nullable()) {
      LOG_ERROR("the field is not nullable");
      return RC::SCHEMA_FIELD_MISSING;
    }

    if (fmeta->type() != value->type && !Stmt::convert_type(fmeta, value)) {
      return RC::SCHEMA_FIELD_TYPE_MISMATCH;
    }
  }

  int condition_num = updates.condition_num;
  const Condition *conditions = updates.conditions;
  for (int i = 0; i < condition_num; i++) {
    const Condition &c = conditions[i];

    if (c.left_is_attr && !c.right_is_attr) {
      const FieldMeta *meta = table->table_meta().field(c.left_attr.attribute_name);
      if (meta == nullptr) {
        return RC::SCHEMA_FIELD_NAME_ILLEGAL;
      }
      if (c.right_value.type == DATES && *(int*)c.right_value.data == -1) {
        return RC::INVALID_ARGUMENT;
      }
      if (!Stmt::check_type(c.right_value.type, meta->type())) {
        return RC::SCHEMA_FIELD_TYPE_MISMATCH;
      }
      // if (c.right_value.type != meta->type() && !Stmt)
      // if (c.right_value.type != meta.field(c.left_attr.attribute_name)->type()) {

      // }
    } else if (c.right_is_attr && !c.left_is_attr) {
      const FieldMeta *meta = table->table_meta().field(c.right_attr.attribute_name);
      if (meta == nullptr) {
        return RC::SCHEMA_FIELD_NAME_ILLEGAL;
      }
      if (c.left_value.type == DATES && *(int*)c.left_value.data == -1) {
        return RC::INVALID_ARGUMENT;
      }
      if (!Stmt::check_type(c.left_value.type, meta->type())) {
        return RC::SCHEMA_FIELD_TYPE_MISMATCH;
      }
      // if (!check_attr_in_table(table, c.right_attr)) {
      //   return RC::SCHEMA_FIELD_NAME_ILLEGAL;
      // }
      // if (c.left_value.type == DATES && *(int*)c.left_value.data == -1) {
      //   return RC::INVALID_ARGUMENT;
      // }
      // if (c.left_value.type != meta.field(c.right_attr.attribute_name)->type()) {
      //   return RC::SCHEMA_FIELD_TYPE_MISMATCH;
      // }
    }
  }
  return RC::SUCCESS;
}

RC ExecuteStage::do_update_table(SQLStageEvent *sql_event)
{
  Updates &updates = sql_event->query()->sstr.update;
  SessionEvent *session_event = sql_event->session_event();
  Session* session=session_event->session();
  Db *db = session->get_current_db();
  Trx *trx = session->current_trx();
  CLogManager *clog_manager = db->get_clog_manager();

  RC rc = check_updates(db, updates);
  if(rc != RC::SUCCESS){
    session_event->set_response("FAILURE\n");
    return rc;
  }

  // inline update table
  Table* table = db->find_table(updates.relation_name);
  int updated_cnt;
  rc = table->update_record(trx, updates.attributes, updates.values, updates.attribute_num, updates.condition_num,
      updates.conditions, &updated_cnt);
  if (rc == RC::SUCCESS) {
    if(!session->is_trx_multi_operation_mode()){
      CLogRecord* clog_record = nullptr;
      rc = clog_manager->clog_gen_record(CLogType::REDO_MTR_COMMIT, trx->get_current_id(), clog_record);
      if(rc != RC::SUCCESS){
        session_event->set_response("FAILURE\n");
        return rc;
      }
      rc = clog_manager->clog_append_record(clog_record);
      if(rc != RC::SUCCESS){
        session_event->set_response("FAILURE\n");
        return rc;
      }
      trx->next_current_id();
      session_event->set_response("SUCCESS\n");
      return rc;
    }
    session_event->set_response("SUCCESS\n");
    return rc;
  } else {
    session_event->set_response("FAILURE\n");
    return rc;
  }
  return rc;
}

RC ExecuteStage::value_check(const int &value_num, const Value *values) const
{
  for (int i = 0; i < value_num; i++) {
    if (values[i].type == DATES && *(const int*)values[i].data == -1) {
      return RC::SCHEMA_FIELD_TYPE_MISMATCH;
    }  }
  return RC::SUCCESS;
}

RC ExecuteStage::compute_value_from_select(Db *db, Value *value)
{
  Stmt *stmt = nullptr;
  RC rc = SelectStmt::create(db, value->select, stmt);
  if (rc != RC::SUCCESS) {
    return rc;
  }
  SelectStmt *select_stmt = dynamic_cast<SelectStmt*>(stmt);
  Pretable *pretable = new Pretable();
  rc = select_to_pretable(select_stmt, std::move(pretable), false, nullptr);
  value->select = nullptr;
  if(rc!=RC::SUCCESS) return rc;
  rc = pretable->assign_row_to_value(value);
  return rc;
}

Pretable *ExecuteStage::Selects_to_pretable(
    Db *db, Value *value, std::unordered_set<Table *> &tables, bool build_exists_results, FieldExpr *exists_left)
{
  Stmt *stmt = nullptr;
   RC rc = SelectStmt::create(db, value->select, stmt, tables);
/*
  std::unordered_set<Table *> empty_parent_tables; // unused `tables` argument
  RC rc = SelectStmt::create(db, value->select, stmt, empty_parent_tables);
*/
  if (rc != RC::SUCCESS) {
    return nullptr;
  }
  SelectStmt *select_stmt = dynamic_cast<SelectStmt*>(stmt);
  // select_stmt->add_parent_tables(tables);
  Pretable *pretable = new Pretable();
  rc = select_to_pretable(select_stmt, std::move(pretable), build_exists_results, exists_left);
  if (rc != RC::SUCCESS) {
    return nullptr;
  }
  return pretable;
}

RC ExecuteStage::do_insert(SQLStageEvent *sql_event)
{
  Stmt *stmt = sql_event->stmt();
  SessionEvent *session_event = sql_event->session_event();
  Session *session = session_event->session();
  Db *db = session->get_current_db();
  Trx *trx = session->current_trx();
  CLogManager *clog_manager = db->get_clog_manager();

  if (stmt == nullptr) {
    LOG_WARN("cannot find statement");
    return RC::GENERIC_ERROR;
  }

  InsertStmt *insert_stmt = (InsertStmt *)stmt;
  Table *table = insert_stmt->table();

  RC rc = table->insert_records(trx, insert_stmt->valuelist_amount(), insert_stmt->valuelists());
  if (rc == RC::SUCCESS) {
    if (!session->is_trx_multi_operation_mode()) {
      CLogRecord *clog_record = nullptr;
      rc = clog_manager->clog_gen_record(CLogType::REDO_MTR_COMMIT, trx->get_current_id(), clog_record);
      if (rc != RC::SUCCESS || clog_record == nullptr) {
        session_event->set_response("FAILURE\n");
        return rc;
      }

      rc = clog_manager->clog_append_record(clog_record);
      if (rc != RC::SUCCESS) {
        session_event->set_response("FAILURE\n");
        return rc;
      }

      trx->next_current_id();
      session_event->set_response("SUCCESS\n");
    } else {
      session_event->set_response("SUCCESS\n");
    }
  } else {
    session_event->set_response("FAILURE\n");
  }
  return rc;
}

RC ExecuteStage::do_delete(SQLStageEvent *sql_event)
{
  Stmt *stmt = sql_event->stmt();
  SessionEvent *session_event = sql_event->session_event();
  Session *session = session_event->session();
  Db *db = session->get_current_db();
  Trx *trx = session->current_trx();
  CLogManager *clog_manager = db->get_clog_manager();

  if (stmt == nullptr) {
    LOG_WARN("cannot find statement");
    return RC::GENERIC_ERROR;
  }

  DeleteStmt *delete_stmt = (DeleteStmt *)stmt;
  TableScanOperator scan_oper(delete_stmt->table());
  PredicateOperator pred_oper(delete_stmt->filter_stmt());
  pred_oper.add_child(&scan_oper);
  DeleteOperator delete_oper(delete_stmt, trx);
  delete_oper.add_child(&pred_oper);

  RC rc = delete_oper.open();
  if (rc != RC::SUCCESS) {
    session_event->set_response("FAILURE\n");
  } else {
    session_event->set_response("SUCCESS\n");
    if (!session->is_trx_multi_operation_mode()) {
      CLogRecord *clog_record = nullptr;
      rc = clog_manager->clog_gen_record(CLogType::REDO_MTR_COMMIT, trx->get_current_id(), clog_record);
      if (rc != RC::SUCCESS || clog_record == nullptr) {
        session_event->set_response("FAILURE\n");
        return rc;
      }

      rc = clog_manager->clog_append_record(clog_record);
      if (rc != RC::SUCCESS) {
        session_event->set_response("FAILURE\n");
        return rc;
      }

      trx->next_current_id();
      session_event->set_response("SUCCESS\n");
    }
  }
  return rc;
}

bool trx_flag;

RC ExecuteStage::do_begin(SQLStageEvent *sql_event)
{
  trx_flag = true;
  RC rc = RC::SUCCESS;
  SessionEvent *session_event = sql_event->session_event();
  Session *session = session_event->session();
  Db *db = session->get_current_db();
  Trx *trx = session->current_trx();
  CLogManager *clog_manager = db->get_clog_manager();

  session->set_trx_multi_operation_mode(true);

  CLogRecord *clog_record = nullptr;
  rc = clog_manager->clog_gen_record(CLogType::REDO_MTR_BEGIN, trx->get_current_id(), clog_record);
  if (rc != RC::SUCCESS || clog_record == nullptr) {
    session_event->set_response("FAILURE\n");
    return rc;
  }

  rc = clog_manager->clog_append_record(clog_record);
  if (rc != RC::SUCCESS) {
    session_event->set_response("FAILURE\n");
  } else {
    session_event->set_response("SUCCESS\n");
  }

  return rc;
}

RC ExecuteStage::do_commit(SQLStageEvent *sql_event)
{
  RC rc = RC::SUCCESS;
  SessionEvent *session_event = sql_event->session_event();
  Session *session = session_event->session();
  Db *db = session->get_current_db();
  Trx *trx = session->current_trx();
  CLogManager *clog_manager = db->get_clog_manager();

  session->set_trx_multi_operation_mode(false);

  CLogRecord *clog_record = nullptr;
  rc = clog_manager->clog_gen_record(CLogType::REDO_MTR_COMMIT, trx->get_current_id(), clog_record);
  if (rc != RC::SUCCESS || clog_record == nullptr) {
    session_event->set_response("FAILURE\n");
    return rc;
  }

  rc = clog_manager->clog_append_record(clog_record);
  if (rc != RC::SUCCESS) {
    session_event->set_response("FAILURE\n");
  } else {
    session_event->set_response("SUCCESS\n");
  }

  trx->next_current_id();

  return rc;
}

RC ExecuteStage::do_clog_sync(SQLStageEvent *sql_event)
{
  RC rc = RC::SUCCESS;
  SessionEvent *session_event = sql_event->session_event();
  Db *db = session_event->session()->get_current_db();
  CLogManager *clog_manager = db->get_clog_manager();

  rc = clog_manager->clog_sync();
  if (rc != RC::SUCCESS) {
    session_event->set_response("FAILURE\n");
  } else {
    session_event->set_response("SUCCESS\n");
  }

  return rc;
}

bool is_null(const TupleCell &cell)
{
  return static_cast<const char*>(cell.data())[cell.length()-1] == 1;
}

TupleSet::TupleSet(const Tuple *t, Table *table) {
  data_ = std::string(t->get_record().data(), table->table_meta().record_size());
  for (int i = 0; i < t->cell_num(); i++) {
    TupleCell cell;
    t->cell_at(i, cell);
    cells_.push_back(cell);
    if (is_null(cell)) {
      LOG_DEBUG("cell %d is null", i);
      cell.set_type(NULLS);
    } else {
      LOG_DEBUG("cell %d is not null", i);
    }
  }
}

TupleSet::TupleSet(const TupleSet *t) {
  cells_ = t->cells_;
  data_ = t->data_;
}

TupleSet *TupleSet::copy() const {
  return new TupleSet(this);
}

// void TupleSet::combine(const TupleSet *t2) {
//   table_num_ += t2->table_num_;
//   for (auto meta : metas_)
//     metas_.push_back(meta);
//   for (auto cell : t2->cells_)
//     cells_.push_back(cell);
// }

TupleSet *TupleSet::generate_combine(const TupleSet *t2) {
  TupleSet *res = this->copy();
  res->data_ += t2->data();
  for (auto cell : t2->cells_) {
    res->cells_.push_back(cell);
  }
  return res;
}

void TupleSet::filter_fields(const std::vector<int> &orders) {
  data_.clear();
  std::vector<TupleCell> cells(orders.size());
  for (size_t i = 0; i < orders.size(); i++) {
    cells[i] = cells_[orders[i]];
    data_ += std::string(cells[i].data(), cells[i].length());
  }
  cells_.swap(cells);
}

// used for aggregate, and they have 5 bytes
void TupleSet::push(const TupleCell &cell)
{
  cells_.push_back(cell);
  data_ += std::string(cell.data(), cell.length());
}

// void TupleSet::push(const TupleCell &cell)
// {
//   cells_.push_back(cell);
// }

// int TupleSet::index(const Field &field) const
// {
//   if (!field.has_table() || !field.has_field()) {
//     return -1;
//   }
//   for (size_t i = 0; i < metas_.size(); i++) {
//     const Field &meta = metas_[i];
//     if (strcmp(meta.table_name(), field.table_name()) == 0 &&
//         strcmp(meta.metac()->name(), field.field_name()) == 0) {
//       return i;
//     }
//   }
//   return -1;
// }

// int TupleSet::index_with_aggr(const Field &field) const
// {
//   if (!field.has_table() || !field.has_field()) {
//     return -1;
//   }
//   for (size_t i = 0; i < metas_.size(); i++) {
//     const Field &meta = metas_[i];
//     if (meta.aggr_type() == field.aggr_type() &&
//         strcmp(meta.table_name(), field.table_name()) == 0 &&
//         strcmp(meta.metac()->name(), field.field_name()) == 0) {
//       return i;
//     }
//   }
//   return -1;
// }

// int TupleSet::index(const Table* table, const FieldMeta& field_meta) const {
//   for (size_t i = 0; i < metas_.size(); i++) {
//     const Field &meta = metas_[i];
//     if (strcmp(meta.table_name(), table->name()) == 0 &&
//         strcmp(meta.metac()->name(), field_meta.name()) == 0) {
//       return i;
//     }
//   }
//   return -1;
// }

TupleCell &TupleSet::get_cell(int idx)
{
  return cells_[idx];
}

// const Field &TupleSet::get_field(int idx)
// {
//   if (idx == -1) {
//     return metas_[0];
//   }
//   return metas_[idx];
// }

// const Field *TupleSet::get_field(const char *table_name, const char *field_name) const
// {
//   for (auto &field : metas_) {
//     if (strcmp(table_name, field.table_name()) == 0 &&
//         strcmp(field_name, field.metac()->name()) == 0) {
//       return &field;
//     }
//   }
//   return nullptr;
// }

// int TupleSet::get_offset(const char *table_name, const char *field_name) const
// {
//   const Field *f = get_field(table_name, field_name);
//   if (f != nullptr) {
//     return f->metac()->offset();
//   }
//   return -1;
// }
//
// const FieldMeta &TupleSet::meta(int idx) const {
//   return *metas_[idx].metac();
// }

bool TupleSet::in(const TupleCell &cell) const
{
  for (auto &c : cells_) {
    if (c.attr_type() == NULLS) {
      continue;
    }
    if (c.compare(cell) == 0) {
      return true;
    }
  }
  return false;
}

bool TupleSet::not_in(const TupleCell &cell) const
{
  for (auto &c : cells_) {
    if (c.attr_type() == NULLS) {
      return false;
    }
    if (c.compare(cell) == 0) {
      return false;
    }
  }
  return true;
}

bool TupleSet::equal(const TupleCell &cell, int offset) const {
  if(offset >= (int)cells_.size()) return false;
  const auto &cell_ = cells_[offset];
  if (cell_.attr_type() == NULLS || cell.attr_type() == NULLS)
    return false;
  if (cell_.compare(cell) == 0)
    return true;
  else
    return false;
}

const std::vector<TupleCell> &TupleSet::cells() const {
  return cells_;
}

// filter table with table-specific conditions
FilterStmt *get_sub_filter(Table *table, FilterStmt *old_filter)
{
  FilterStmt *filter = new FilterStmt();
  for (FilterUnit *unit : old_filter->filter_units()) {
    Expression *left = unit->left();
    Expression *right = unit->right();
    if (left->type() == ExprType::FIELD && right->type() == ExprType::FIELD) {
      // note：所以针对 “select ssq_2.id from ssq_2 where ssq_1.id > ssq_2.id;” 之类的unit，总是被过滤掉，因为ssq_1.id未知。
      continue;
    }
    if (left->type() == ExprType::VALUE && right->type() == ExprType::VALUE) {
      filter->push(unit);
      continue;
    }
    if (left->type() == ExprType::VALUE && right->type() == ExprType::FIELD) {
      std::swap(left, right);
    }
    FieldExpr &left_field_expr = *(FieldExpr *)left;
    if (strcmp(table->name(), left_field_expr.table_name()) != 0 ||
        table->table_meta().field(left_field_expr.field_name()) == nullptr) {
      continue;
    }
    ValueExpr *right_value_expr = dynamic_cast<ValueExpr*>(right);
    if (right_value_expr->get_type() == AttrType::SELECTS) {

    }
    filter->push(unit);
  }
  return filter;
}

Pretable::Pretable(ValueList *valuelist)
{
  TupleSet tupleset;
  for (int i = 0; i < valuelist->value_num; i++) {
    Value &value = valuelist->values[i];
    TupleCell cell(value.type, (char *)value.data);
    switch (value.type) {
      case CHARS:
        cell.set_length(strlen((char *)value.data) + 2);
        break;
      default:
        cell.set_length(5);
    }
    tupleset.push(cell);
  }
  groups_.resize(1);
  groups_[0].push_back(tupleset);
}

bool Pretable::in(Value *value) const
{
  TupleCell cell(value->type, (char *)value->data);
  for (auto &group : groups_) {
    for (auto &tuple : group) {
      if (tuple.in(cell)) {
        return true;
      }
    }
  }
  return false;
}

bool Pretable::in(TupleCell &cell) const
{
  if (cell.attr_type() == NULLS) {
    return false;
  }
  for (auto &group : groups_) {
    for (auto &tuple : group) {
      if (tuple.in(cell)) {
        return true;
      }
    }
  }
  return false;
}

bool Pretable::not_in(TupleCell &cell) const
{
  if (cell.attr_type() == NULLS) {
    return false;
  }
  for (auto &group : groups_) {
    for (auto &tuple : group) {
      if (!tuple.not_in(cell)) {
        return false;
      }
    }
  }
  return true;
}

bool Pretable::exists(TupleCell &cell) const
{
  for (const auto &item : exists_results) {
    if(item.compare(cell) == 0)
      return true;
  }
  return false;
}
bool Pretable::not_exists(TupleCell &cell) const
{
  return !exists(cell);
}

Pretable::Pretable(const Pretable& t)
    :groups_(),tables_()
{
  std::vector<TupleSet> row_buf;
  for (const auto &  row: t.groups_) {
    for(const auto & item: row){
         row_buf.push_back(item);
    }
    groups_.push_back(row_buf);
    row_buf.clear();
  }
  for (const auto & item: t.tables_) {
    tables_.push_back(item);
  }
}

Pretable::Pretable(Pretable&& t)
    :groups_(std::move(t.groups_)),
      tables_(std::move(t.tables_))
{}

Pretable& Pretable::operator=(Pretable&& t)
{
  groups_ = std::move(t.groups_);
  tables_ = std::move(t.tables_);
  return *this;
}

RC Pretable::init(Table *table, FilterStmt *old_filter)
{
  FilterStmt *filter = get_sub_filter(table, old_filter);
  tables_.push_back(table);

  Operator *scan_oper = new TableScanOperator(table);
  DEFER([&] () {delete scan_oper;});

  // add fields
  for (const auto &meta : *table->table_meta().field_metas()) {
    Field f(table, meta.copy());
    fields_.push_back(f);
  }

  // first get a subset of filter
  PredicateOperator pred_oper(filter);
  pred_oper.add_child(scan_oper);

  RC rc = RC::SUCCESS;
  rc = pred_oper.open();
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to open operator");
    return rc;
  }

  groups_.resize(1);
  while ((rc = pred_oper.next()) == RC::SUCCESS) {
    // get current record
    // write to response
    Tuple *tuple = pred_oper.current_tuple();
    if (nullptr == tuple) {
      rc = RC::INTERNAL;
      LOG_WARN("failed to get current record. rc=%s", strrc(rc));
      break;
    }
    groups_[0].push_back(new TupleSet(tuple, table));
  }
  // delete filter;

  return rc;
}

RC Pretable::exists(Pretable* pre2, FilterStmt* filter) {
  if(filter == nullptr || filter->filter_units().size() == 0) return RC::SUCCESS;
  if(filter->filter_units().size() > 1) return RC::UNIMPLENMENT;
  auto filter_unit0=filter->filter_units()[0];
  RC rc = RC::SUCCESS;
  if(!(filter_unit0->comp()==VALUE_EXISTS || filter_unit0->comp()==VALUE_NOT_EXISTS)) return rc;
  if(pre2 == nullptr)
    pre2 = dynamic_cast<ValueExpr*>(filter_unit0->right())->pretable();

  if(pre2->fields_.size() > 1) return RC::UNIMPLENMENT;
  const char *target_field_name = dynamic_cast<FieldExpr*>(filter_unit0->left())->field_name();
  auto &groups11 = groups_[0];
  // find column id
  int target_field_offset1 = -1, target_field_offset2 = -1;
  for (size_t i = 0; i < fields_.size(); ++i) {
    if(strcmp(fields_[i].field_name(), target_field_name) == 0) {
      target_field_offset1 = i;
      break;
    }
  }
  const auto &groups21 = pre2->groups_[0];
  std::vector<TupleSet> groups11_filtered;
  for(auto ts1 = groups11.begin(); ts1 != groups11.end(); ++ts1) {
    bool hit = false;
    for(const auto &ts2 : groups21) {
      hit = ts2.equal(ts1->cells()[target_field_offset1], target_field_offset2);
      hit = filter_unit0->comp() == VALUE_EXISTS ? hit : !hit;
      if(hit) break;
    }
    if (hit)
      groups11_filtered.push_back(*ts1);
  }
  groups_[0] = groups11_filtered;
  return rc;
}

// before aggregation
const Field *Pretable::field(const Field &field) const
{
  for (auto &f : fields_) {
    if (strcmp(f.table_name(), field.table_name()) == 0 &&
        strcmp(f.metac()->name(), field.metac()->name()) == 0) {
      return &f;
    }
  }
  return nullptr;
}

const Field *Pretable::field(const char *table_name, const char *field_name) const
{
  for (const auto &f : fields_) {
    if (strcmp(f.table_name(), table_name) == 0 &&
        strcmp(f.metac()->name(), field_name) == 0) {
      return &f;
    }
  }
  return nullptr;
}

const Field *Pretable::field_with_aggr(const char *table_name, const char *field_name, AggreType type) const
{
  for (const auto &f : fields_) {
    if (strcmp(f.table_name(), table_name) == 0 &&
        strcmp(f.metac()->name(), field_name) == 0 &&
        f.aggr_type() == type) {
      return &f;
    }
  }
  return nullptr;
}

const FieldMeta *Pretable::field_meta(const Field &field) const {

  const Field *p_field = this->field(field);
  if (p_field) {
    return p_field->metac();
  }
  return nullptr;
}

RC Pretable::aggregate_max(int idx, TupleCell *res, int group_id)
{
  LOG_INFO("aggregate max");
  std::vector<TupleSet> &group = groups_[group_id];
  const FieldMeta *meta = field_meta(idx);
  size_t len = meta->len();
  res->set_length(len);
  char *data = new char[len];
  memset(data, 0, len);

  const TupleCell *tmp = &group[0].get_cell(idx);

  for (TupleSet &tuple : group) {
    const TupleCell &cell = tuple.get_cell(idx);
    if (cell.attr_type() != NULLS) {
      if (tmp->attr_type() == NULLS) {
        tmp = &cell;
      } else {
        int comp = cell.compare(*tmp);
        if (comp > 0) {
          tmp = &cell;
        }
      }
    }
  }
  if (tmp->attr_type() == NULLS) {
    res->set_type(NULLS);
    data[len-1] = 1;
  } else {
    res->set_type(meta->type());
    memcpy(data, tmp->data(), len-1);
  }
  res->set_data(data);
  return RC::SUCCESS;
}

RC Pretable::aggregate_min(int idx, TupleCell *res, int group_id)
{
  LOG_INFO("aggregate min");
  std::vector<TupleSet> &group = groups_[group_id];
  const FieldMeta *meta = field_meta(idx);
  size_t len = meta->len();
  res->set_length(len);
  char *data = new char[len];
  memset(data, 0, len);

  const TupleCell *tmp = &group[0].get_cell(idx);
  for (TupleSet &tuple : group) {
    const TupleCell &cell = tuple.get_cell(idx);
    if (cell.attr_type() != NULLS) {
      if (tmp->attr_type() == NULLS) {
        tmp = &cell;
      } else {
        int comp = tmp->compare(cell);
        if (comp > 0) {
          tmp = &cell;
        }
      }
    }
  }
  if (tmp->attr_type() == NULLS) {
    res->set_type(NULLS);
    data[len-1] = 1;
  } else {
    res->set_type(meta->type());
    memcpy(data, tmp->data(), len-1);
  }
  res->set_data(data);
  return RC::SUCCESS;
}

RC Pretable::aggregate_count(int idx, TupleCell *res, int group_id)
{
  LOG_INFO("aggregate count");
  std::vector<TupleSet> &group = groups_[group_id];
  int ans = 0;
  size_t len = sizeof(int) + 1;
  char *data = new char[len];
  memset(data, 0, len);
  if (idx == -1) {
    ans = group.size();
  } else {
    for (TupleSet &tuple : group) {
      const TupleCell &cell = tuple.get_cell(idx);
      if (cell.attr_type() != NULLS) {
        ans++;
      }
    }
  }

  memcpy(data, &ans, sizeof(int));
  res->set_type(INTS);
  res->set_length(len);
  res->set_data(data);
  return RC::SUCCESS;
}

RC Pretable::aggregate_sum(int idx, TupleCell *res, int group_id)
{
  LOG_INFO("aggregate sum");
  std::vector<TupleSet> &group = groups_[group_id];
  float ans = 0;
  AttrType type = UNDEFINED;
  int cnt = 0;
  for (TupleSet &tuple : group) {
    const TupleCell &cell = tuple.get_cell(idx);
    type = cell.attr_type();
    switch (cell.attr_type()) {
      case INTS: {
        ans += *(int *)cell.data();
        cnt++;
      } break;
      case FLOATS: {
        ans += *(float *)cell.data();
        cnt++;
      } break;
      case CHARS: {
        ans += Stmt::char_to_float(cell.data());
        cnt++;
      } break;
      case NULLS: {
        continue;
      }
      case DATES:
      default: {
        return RC::INTERNAL;
      }
    }
  }
  res->set_type(type);
  size_t len = sizeof(float)+1;
  res->set_length(len);
  char *data = new char[len];
  memset(data, 0, len);
  if (cnt == 0) {
    res->set_type(NULLS);
    data[len - 1] = 1;
  } else if (type == INTS) {
    int a = ans;
    memcpy(data, &a, sizeof(int));
  } else if (type == FLOATS || type == CHARS) {
    memcpy(data, &ans, sizeof(float));
  }
  res->set_data(data);
  return RC::SUCCESS;
}

RC Pretable::aggregate_avg(int idx, TupleCell *res, int group_id)
{
  LOG_INFO("aggregate avg");
  std::vector<TupleSet> &group = groups_[group_id];
  float ans = 0;
  int cnt = 0;
  for (TupleSet &tuple : group) {
    const TupleCell &cell = tuple.get_cell(idx);
    switch (cell.attr_type()) {
      case INTS: {
        ans += *(int *)cell.data();
        cnt++;
      } break;
      case FLOATS: {
        ans += *(float *)cell.data();
        cnt++;
      } break;
      case CHARS: {
        ans += Stmt::char_to_float(cell.data());
        cnt++;
      } break;
      case NULLS: {
        continue;
      }
      case DATES:
      default: {
        return RC::INTERNAL;
      }
    }
  }
  size_t len = sizeof(float)+1;
  char *data = new char[len];
  memset(data, 0, len);
  res->set_length(len);
  if (cnt == 0) {
    data[len - 1] = 1;
    res->set_type(NULLS);
  } else {
    ans /= cnt;
    memcpy(data, &ans, len-1);
    res->set_type(FLOATS);
  }
  res->set_data(data);
  return RC::SUCCESS;
}

// before aggregate
int Pretable::index(const Field &field) const
{
  for (size_t i = 0; i < fields_.size(); i++) {
    const Field &f = fields_[i];
    if (strcmp(f.table_name(), field.table_name()) == 0 &&
        strcmp(f.metac()->name(), field.metac()->name()) == 0) {
      return i;
    }
  }
  return -1;
}

int Pretable::index(const char *table_name, const char *field_name) const
{
  for (size_t i = 0; i < fields_.size(); i++) {
    const Field &f = fields_[i];
    if (strcmp(f.table_name(), table_name) == 0 &&
        strcmp(f.metac()->name(), field_name) == 0) {
      return i;
    }
  }
  return -1;
}

void Pretable::groupby(const std::vector<Field> groupby_fields)
{
  for (const auto &field : groupby_fields) {
    PretableHash hash(field.attr_type());
    std::vector<std::vector<TupleSet>> groups;
    int idx = index(field);
    for (auto &group : groups_) {
      std::vector<std::vector<TupleSet>> sub_groups;
      for (auto &tuple : group) {
        int group_idx = hash.get_value(tuple.get_cell(idx));
        if (group_idx >= (int)sub_groups.size()) {
          sub_groups.resize(group_idx + 1);
        }
        sub_groups[group_idx].push_back(tuple);
      }
      groups.insert(groups.end(), sub_groups.begin(), sub_groups.end());
    }
    groups_.swap(groups);
  }
}

void Pretable::having(Condition *having_conditions, int having_condition_num)
{
  if (having_condition_num == 0) {
    return;
  }
  CompositeConditionFilter *filter = make_having_filter(having_conditions, having_condition_num);

  std::vector<std::vector<TupleSet>> groups;
  for (auto &group : groups_) {
    assert(group.size() == 1);
    TupleSet &tuple = group[0];
    Record rec;
    char *buf = new char[tuple.data().size()];
    memcpy(buf, tuple.data().c_str(), tuple.data().size());
    rec.set_data(buf);
    if (filter->filter(rec)) {
      groups.push_back(group);
    }
  }
  groups_.swap(groups);
}

int Pretable::tuple_num() const {
  int ans = 0;
  for (auto &group : groups_) {
    ans += group.size();
  }
  return ans;
}

// after aggregation, there are original field and aggregation field in the tuple
RC Pretable::aggregate(std::vector<Field> fields)
{
  LOG_INFO("begin aggregate");
  if (groups_.size() == 0) {
    return RC::SUCCESS;
  }

  RC rc = RC::SUCCESS;
  for (size_t i = 0; i < groups_.size(); i++) {
    std::vector<TupleSet> &group = groups_[i];
    if (group.size() == 0) {
      continue;
    }
    TupleSet res;
    for (auto &field : fields) {
      int idx = index(field);
      TupleCell cell;
      if (field.aggr_type() == AggreType::A_NO) {
        cell = group[0].get_cell(idx);
      } else if (idx == -1 && field.aggr_type() != AggreType::A_COUNT) {
        LOG_INFO("log i don't know");
        cell.set_type(AttrType::CHARS);
        cell.set_length(strlen(field.metac()->name()) + 2);
        cell.set_data(field.metac()->name());
      }  else {
        switch (field.aggr_type()) {
          case A_MAX:
            rc = aggregate_max(idx, &cell, i);break;
          case A_MIN:
            rc = aggregate_min(idx, &cell, i);break;
          case A_AVG:
            rc = aggregate_avg(idx, &cell, i);break;
          case A_COUNT:
            rc = aggregate_count(idx, &cell, i);break;
          case A_SUM:
            rc = aggregate_sum(idx, &cell, i); break;
          case A_FAILURE:
            return RC::SCHEMA_FIELD_REDUNDAN;
          default:
            LOG_ERROR("WTF");
            return RC::GENERIC_ERROR;
        }
        LOG_INFO("CELL type %d, len %d", cell.attr_type(), cell.length());
        if (rc != SUCCESS) {
          LOG_ERROR("wrong wrong wrong");
          return rc;
        }
      }
      res.push(cell);
    }
    group.clear();
    group.push_back(res);
  }
  int offset = 0;
  fields_.clear();
  for (const auto &f : fields) {
    FieldMeta *field_meta = f.metac()->copy();
    field_meta->set_offset(offset);
    offset += field_meta->len();
    Field field(f.table(), field_meta);
    field.set_aggr(f.aggr_type());
    fields_.push_back(field);
  }
  return RC::SUCCESS;
}

const FieldMeta *Pretable::field_meta(const char *table_name, const char *field_name) const
{
  const Field *f = this->field(table_name, field_name);
  return f->metac();
}

int Pretable::get_offset(const char *table_name, const char *field_name) const
{
  const FieldMeta *meta = this->field_meta(table_name, field_name);
  return meta->offset();
}

// if table is null, then it's this table, else its table outside
ConDesc Pretable::make_cond_desc(Expression *expr)
{
  ConDesc desc;
  if (expr->type() == ExprType::FIELD) {
    desc.is_attr = true;
    FieldExpr *field_expr = dynamic_cast<FieldExpr*>(expr);
    desc.attr_length = field_expr->field().meta()->len();
    std::cout << "tuple size: " << groups_[0][0].size() << std::endl;
    // const FieldMeta *field_meta = this->field_meta(field_expr->field());
    desc.attr_offset = get_offset(field_expr->table_name(), field_expr->field_name());
    // if (field(field_expr->field()) != nullptr) {

    // } else {
    //   desc.attr_offset = groups_[0][0].size() + t2->groups_[0][0].get_offset(field_expr->table_name(), field_expr->field_name());
    // }
  } else {
    desc.is_attr = false;
    ValueExpr *value_expr = dynamic_cast<ValueExpr*>(expr);
    desc.value = value_expr->get_data();
  }
  return desc;
}

CompositeConditionFilter *Pretable::make_having_filter(Condition *conditions, int num)
{
  if (num == 0) {
    return nullptr;
  }

  ConditionFilter **filters = new ConditionFilter*[num];

  for (int i = 0; i < num; i++) {
    Condition &cond = conditions[i];
    ConDesc left, right;
    AttrType left_type, right_type;
    memset(&left, 0, sizeof(ConDesc));
    memset(&right, 0, sizeof(ConDesc));
    left.is_attr = cond.left_is_attr;
    right.is_attr = cond.right_is_attr;

    if (cond.left_is_attr) {
      RelAttr &attr = cond.left_attr;
      const Field *field = this->field(attr.relation_name, attr.attribute_name);
      left_type = field->metac()->type();
      left.attr_length = field->metac()->len();
      left.attr_offset = field->metac()->offset();
    } else {
      left_type = cond.left_value.type;
      left.value = cond.left_value.data;
    }

    if (cond.right_is_attr) {
      RelAttr &attr = cond.right_attr;
      const Field *field = this->field(attr.relation_name, attr.attribute_name);
      right_type = field->metac()->type();
      right.attr_length = field->metac()->len();
      right.attr_offset = field->metac()->offset();
    } else {
      right_type = cond.right_value.type;
      right.value = cond.right_value.data;
    }

    filters[i] = new DefaultConditionFilter();
    dynamic_cast<DefaultConditionFilter*>(filters[i])->init(left, right, left_type, right_type, cond.comp);
  }
  CompositeConditionFilter *ans = new CompositeConditionFilter();

  ans->init((const ConditionFilter**)filters, num);
  return ans;
}

// combine two tupleset -> record, then filter
CompositeConditionFilter *Pretable::make_cond_filter(std::vector<FilterUnit*> &units)
{
  int n = units.size();
  if (n == 0) {
    return nullptr;
  }
  ConditionFilter **filters = new ConditionFilter*[n];

  for (int i = 0; i < n; i++) {
    ConDesc left = make_cond_desc(units[i]->left());
    ConDesc right = make_cond_desc(units[i]->right());
    AttrType left_type, right_type;
    if (units[i]->left()->type() == ExprType::FIELD) {
      left_type = dynamic_cast<FieldExpr*>(units[i]->left())->field().attr_type();
    } else {
      left_type = dynamic_cast<ValueExpr*>(units[i]->left())->get_type();
    }
    if (units[i]->right()->type() == ExprType::FIELD) {
      right_type = dynamic_cast<FieldExpr*>(units[i]->right())->field().attr_type();
    } else {
      right_type = dynamic_cast<ValueExpr*>(units[i]->right())->get_type();
    }

    // incomparable type is checked in filter stmt
    filters[i] = new DefaultConditionFilter();
    dynamic_cast<DefaultConditionFilter*>(filters[i])->init(left, right, left_type, right_type, units[i]->comp());
  }
  CompositeConditionFilter *ans = new CompositeConditionFilter();

  ans->init((const ConditionFilter**)filters, n);
  return ans;
}

RC Pretable::join(Pretable *pre2, FilterStmt *filter)
{
  std::vector<FilterUnit*> units;

  for (FilterUnit *unit : filter->filter_units()) {
    Expression *left = unit->left();
    Expression *right = unit->right();
    if (left->type() == ExprType::VALUE || right->type() == ExprType::VALUE) {
      continue;
    }
    FieldExpr *left_field_expr = dynamic_cast<FieldExpr*>(left);
    FieldExpr *right_field_expr = dynamic_cast<FieldExpr*>(right);
    if (this->field(left_field_expr->field()) != nullptr &&
        pre2->field(right_field_expr->field()) != nullptr) {
      units.push_back(unit);
    } else if (this->field(right_field_expr->field()) != nullptr &&
               pre2->field(left_field_expr->field()) != nullptr) {
      units.push_back(unit);
    }
  }

  if (pre2->group_num() == 0 || pre2->groups_[0].size() == 0 ||
      groups_.size() == 0 || groups_[0].size() == 0) {
    groups_.clear();
    return RC::SUCCESS;
  }

  // combine fields
  int off = 0;
  for (auto &field : fields_) {
    off += field.metac()->len();
  }
  for (auto &field : pre2->fields_) {
    field.meta()->set_offset(off + field.meta()->offset());
    fields_.push_back(field);
  }

  // combine tupleset
  CompositeConditionFilter *cond_filter = make_cond_filter(units);
  std::vector<TupleSet> &tuple1 = groups_[0];
  std::vector<TupleSet> &tuple2 = pre2->groups_[0];
  std::vector<TupleSet> res;
  if (cond_filter != nullptr) {
    for (TupleSet &t1 : tuple1) {
      for (TupleSet &t2 : tuple2) {
        TupleSet *tuple = t1.generate_combine(&t2);
        Record rec;
        char *buf = new char[tuple->data().size()];
        memcpy(buf, tuple->data().c_str(), tuple->data().size());
        rec.set_data(buf);
        if (cond_filter->filter(rec)) {
          res.push_back(*tuple);
        }
      }
    }
  } else {
    for (TupleSet &t1 : tuple1) {
      for (TupleSet &t2 : tuple2) {
        res.push_back(*t1.generate_combine(&t2));
      }
    }
  }

  for (Table *t : pre2->tables_) {
    tables_.push_back(t);
  }
  groups_.clear();
  groups_.push_back(res);
  return RC::SUCCESS;
}

std::string aggr_to_string(AggreType type) {
  switch (type) {
    case A_MAX:
      return "max";
    case A_MIN:
      return "min";
    case A_AVG:
      return "avg";
    case A_COUNT:
      return "count";
    case A_SUM:
      return "sum";
    default:
      return "";
  }
}

void ExecuteStage::print_fields(std::stringstream &ss, const std::vector<Field> &fields, bool multi, int num) {
  bool first = true;
  for (int i = 0; i < num; i++) {
    const Field &field = fields[i];
    ss << (first ? "" : " | ");
    first = false;
    // std::string tp = field.has_field() ? field.field_name() : field.aggr_str();
    std::string tp = field.field_name();
    if (field.should_print_table() ||
        (multi && field.has_table())) {
      tp = field.table_name() + ("." + tp);
    }
    if (field.aggr_type() != A_NO) {
      tp = aggr_to_string(field.aggr_type()) + '(' + tp + ')';
    }
    ss << tp;
  }

  if (!first) {
    ss << '\n';
  }
}

// haven't changed field offset
void Pretable::filter_fields(const std::vector<Field> &fields) {
  std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<AggreType, int>>> mp;
  std::vector<Field> new_fields(fields.size());

  for (size_t i = 0; i < fields.size(); i++) {
    mp[fields[i].table_name()][fields[i].field_name()][fields[i].aggr_type()] = i+1;
  }

  std::vector<int> orders(fields.size());
  for (size_t i = 0; i < fields_.size(); i++) {
    auto &f = fields_[i];
    int j = mp[f.table_name()][f.meta()->name()][f.aggr_type()];
    if (j > 0) {
      orders[j-1] = i;
      // new_fields[j-1] = fields_[i];
    }
  }

  fields_ = fields;

  for (auto &group : groups_) {
    for (auto &tuple : group) {
      tuple.filter_fields(orders);
    }
  }
}

void Pretable::order_by(const std::vector<OrderByField> &order_by_fields){
  if(order_by_fields.empty()){
    return;
  }
  for (auto &group : groups_) {
    if(group.empty()){
      return;
    }
    std::vector<std::pair<int,int>> index_desc_pairs;
    for(auto &order_by_field : order_by_fields){
      int index = this->index(order_by_field.table->name(), order_by_field.field_meta->name());
      // int index = group[0].index(order_by_field.table, *order_by_field.field_meta);
      index_desc_pairs.push_back(std::make_pair(index, order_by_field.is_desc));
    }
    std::reverse(index_desc_pairs.begin(), index_desc_pairs.end());
    sort(group.begin(), group.end(), [&](TupleSet& a, TupleSet& b) -> bool {
      for(auto i_d: index_desc_pairs){
        auto& index = i_d.first;
        auto& is_desc = i_d.second;
        TupleCell va = a.get_cell(index);
        TupleCell vb = b.get_cell(index);
        auto res = va.compare(vb);
        if(res == 0){
          continue;
        }
        if (is_desc) {
          return res>0;
        } else {
          return res<0;
        }
      }
      return false;
    });
  }

}

void Pretable::print(std::stringstream &ss, int num)
{
  for (auto &group : groups_) {
    for (const TupleSet &tuple : group) {
      bool first = true;
      for (int i = 0; i < num; i++) {
        const TupleCell &cell = tuple.cells()[i];
        if (!first) {
          ss << " | ";
        } else {
          first = false;
        }
        cell.to_string(ss);
      }
      ss << '\n';
    }
  }
}

RC Pretable::assign_row_to_value(Value *value)
{
  if (only_one_cell()) {
    TupleSet &tuple = groups_[0][0];
    const FieldMeta *meta = this->field_meta(0);
    value->type = tuple.get_cell(0).attr_type();
    value->data = new char[meta->len()];
    memcpy(value->data, tuple.get_cell(0).data(), meta->len());
    // null case
    if (((char *)value->data)[meta->len() - 1] == 1) {
      value->type = NULLS;
    }
    return RC::SUCCESS;
  }
  return RC::SCHEMA_FIELD_TYPE_MISMATCH;
}

// this is before group by
bool Pretable::valid_operation(CompOp op) const {
  if (groups_[0].size() == 0 || op == VALUE_EXISTS || op == VALUE_NOT_EXISTS) {
    return true;
  }
  if (op == VALUE_IN || op == VALUE_NOT_IN) {
    return groups_[0][0].cells().size() == 1;
  }
  return only_one_cell();
}

PretableHash::PretableHash(AttrType type) : type_(type), index_(0), null_index_(-1) {
  switch (type) {
    case INTS: case DATES:
      map_ = new std::unordered_map<int, int>;
      break;
    case CHARS:
      map_ = new std::unordered_map<std::string, int>;
      break;
    case FLOATS:
      map_ = new std::unordered_map<float, int>;
    default:
      LOG_ERROR("wtf");
      map_ = nullptr;
  }
}

int PretableHash::get_value(const TupleCell &cell) {
  switch (cell.attr_type()) {
    case INTS: case DATES: {
      auto map = static_cast<std::unordered_map<int, int> *>(map_);
      int val = *(int*)cell.data();
      if (!map->count(val)) {
        (*map)[val] = index_++;
      }
      return (*map)[val];
    }
    case CHARS: {
      auto map = static_cast<std::unordered_map<std::string, int> *>(map_);
      std::string val = cell.data();
      if (!map->count(val)) {
        (*map)[val] = index_++;
      }
      return (*map)[val];
    }
    case FLOATS: {
      auto map = static_cast<std::unordered_map<float, int> *>(map_);
      int val = *(float*)cell.data();
      if (!map->count(val)) {
        (*map)[val] = index_++;
      }
      return (*map)[val];
    }
    case NULLS:
      if (null_index_ == -1) {
        null_index_ = index_++;
      }
      return null_index_;
    default:
      return -1;
  }

}

RC ExecuteStage::do_update(SQLStageEvent *sql_event)
{
  Stmt *stmt = sql_event->stmt();
  SessionEvent *session_event = sql_event->session_event();
  Session *session = session_event->session();
  Db *db = session->get_current_db();
  Trx *trx = session->current_trx();
  CLogManager *clog_manager = db->get_clog_manager();

  if (stmt == nullptr) {
    LOG_WARN("cannot find statement");
    session_event->set_response("FAILURE\n");
    return RC::GENERIC_ERROR;
  }

  UpdateStmt *update_stmt = (UpdateStmt *)stmt;
  Table *table = update_stmt->table();

  RC rc = RC::SUCCESS;

  std::vector<Value *> &values = update_stmt->values();
  std::vector<const FieldMeta *> &metas = update_stmt->metas();
  for (size_t i  = 0; i < values.size(); i++) {
    if (values[i]->type == AttrType::SELECTS) {
      rc = compute_value_from_select(db, values[i]);
      if (rc != RC::SUCCESS) {
        session_event->set_response("FAILURE\n");
        LOG_DEBUG("selects in update isn't good");
        return rc;
      }
    }
  }
  Operator *scan_oper = try_to_create_index_scan_operator(nullptr);
  if (nullptr == scan_oper) {
    scan_oper = new TableScanOperator(update_stmt->table());
  }

  DEFER([&] () {delete scan_oper;});

  PredicateOperator pred_oper(nullptr);
  pred_oper.add_child(scan_oper);

  rc = pred_oper.open();
  if (rc != RC::SUCCESS) {
    session_event->set_response("FAILURE\n");
    LOG_WARN("failed to open operator");
    return rc;
  }
  while ((rc = pred_oper.next()) == RC::SUCCESS) {
    // get current record
    // write to response
    RowTuple *tuple = dynamic_cast<RowTuple*>(pred_oper.current_tuple());
    if (nullptr == tuple) {
      rc = RC::INTERNAL;
      LOG_WARN("failed to get current record. rc=%s", strrc(rc));
      break;
    }
    Record record = tuple->record();
    for (size_t i = 0; i < values.size(); i++) {
      const FieldMeta *field = metas[i];
      Value *value = values[i];
      // prepare data
      size_t copy_len = field->len();
      if (field->type() == CHARS) {
        const size_t data_len = strlen((const char *)value->data);
        if (copy_len > data_len) {
          copy_len = data_len + 1;
        }
      }
      memcpy(record.data() + field->offset(), value->data, copy_len);

      // update
      rc = table->update_record(trx, &record, false);
      if(rc == RC::SUCCESS) {
        if(!session->is_trx_multi_operation_mode()) {
          CLogRecord *clog_record = nullptr;
          rc = clog_manager->clog_gen_record(CLogType::REDO_MTR_COMMIT, trx->get_current_id(), clog_record);
          if (rc != RC::SUCCESS || clog_record == nullptr) {
            session_event->set_response("FAILURE\n");
            return rc;
          }
          rc = clog_manager->clog_append_record(clog_record);
          if (rc != RC::SUCCESS) {
            session_event->set_response("FAILURE\n");
            return rc;
          }
          trx->next_current_id();
          session_event->set_response("SUCCESS\n");
        } else {
          session_event->set_response("SUCCESS\n");
        }
      } else {
        session_event->set_response("FAILURE\n");
        return rc;
      }
    } // for
  } // while

  if (rc != RC::RECORD_EOF) {
    LOG_WARN("something wrong while iterate operator. rc=%s", strrc(rc));
    pred_oper.close();
  } else {
    rc = pred_oper.close();
  }
  if (rc == RC::SUCCESS) {
    session_event->set_response("SUCCESS\n");
  } else {
    session_event->set_response("FAILURE\n");
  }
  return rc;
}
