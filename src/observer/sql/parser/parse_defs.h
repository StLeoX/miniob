/* Copyright (c) 2021 Xie Meiyi(xiemeiyi@hust.edu.cn) and OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

// by StLeoX

#ifndef __OBSERVER_SQL_PARSER_PARSE_DEFS_H__
#define __OBSERVER_SQL_PARSER_PARSE_DEFS_H__

#include <stddef.h>

#define MAX_NUM 20
#define MAX_REL_NAME 20
#define MAX_ATTR_NAME 20
#define MAX_ERROR_MESSAGE 20
#define MAX_DATA 50

// 聚合函数类型
typedef enum
{
  A_NO,
  A_MAX,
  A_MIN,
  A_AVG,
  A_COUNT,
  A_FAILURE,
  A_SUM
} AggreType;

// 属性
typedef struct {
  char *relation_name;   // relation name (may be NULL) 表名
  char *attribute_name;  // attribute name              属性名
  AggreType type;
  int print_attr;
} RelAttr;

// 表达式算符类型
typedef enum {
  EQUAL_TO,     //"="     0
  LESS_EQUAL,   //"<="    1
  NOT_EQUAL,    //"<>"    2
  LESS_THAN,    //"<"     3
  GREAT_EQUAL,  //">="    4
  GREAT_THAN,   //">"     5
  IS_EQUAL,
  IS_NOT_EQUAL,
  STR_LIKE,
  STR_NOT_LIKE,
  VALUE_IN,
  VALUE_NOT_IN,
  VALUE_EXISTS,
  VALUE_NOT_EXISTS,
  NO_OP
} CompOp;

// 表达式类型
typedef enum { ADDN, SUBN, MULN, DIVN, VALN, ATTRN } ExpType;

typedef struct ast {
  ExpType type;
  int l_brace;
  int r_brace;
  struct ast *l;
  struct ast *r;
} ast;

// 属性值类型
typedef enum
{
  UNDEFINED,
  CHARS,
  INTS,
  DATES,
  NULLS,
  SELECTS,
  VALUELIST,
  TEXTS,     // text don't have null bit
  FLOATS
} AttrType;

struct _Selects;
struct _ValueList;

//属性值
typedef struct _Value {
  AttrType type;  // type of value
  void *data;     // value
  struct _Selects *select; // 未计算的select子句
  struct _ValueList *value_list;
} Value;

typedef struct _ValueList{
  int value_num;
  Value values[MAX_NUM];
} ValueList;

// 条件表达式
typedef struct _Condition {
  int left_is_attr;    // TRUE if left-hand side is an attribute
                       // 1时，操作符左边是属性名，0时，是属性值
  Value left_value;    // left-hand side value if left_is_attr = FALSE
  RelAttr left_attr;   // left-hand side attribute
  CompOp comp;         // comparison operator
  int right_is_attr;   // TRUE if right-hand side is an attribute
                       // 1时，操作符右边是属性名，0时，是属性值
  RelAttr right_attr;  // right-hand side attribute if right_is_attr = TRUE 右边的属性
  Value right_value;   // right-hand side value if right_is_attr = FALSE
} Condition;

// struct of orderby
typedef struct {
  RelAttr attr;
  size_t is_desc;
} OrderByRelAttr;

// struct of select
typedef struct _Selects {
  size_t attr_num;                // Length of attrs in Select clause
  RelAttr attributes[MAX_NUM];    // attrs in Select clause
  size_t attr_exp_num;
  ast *attributes_exp[MAX_NUM];
  size_t relation_num;            // Length of relations in Fro clause
  char *relations[MAX_NUM];       // relations in From clause
  size_t condition_num;           // Length of conditions in Where clause
  Condition conditions[MAX_NUM];  // conditions in Where clause
  size_t aggregate_num;
  size_t order_by_num;
  OrderByRelAttr order_fields[MAX_NUM];
  RelAttr groupby_attrs[MAX_NUM];
  size_t groupby_num;
  Condition having_conditions[MAX_NUM];
  size_t having_num;
} Selects;

// struct of insert
typedef struct {
  char *relation_name;    // Relation to insert into
  size_t valuelist_num;
  ValueList valuelist[MAX_NUM];
} Inserts;

// struct of delete
typedef struct {
  char *relation_name;            // Relation to delete from
  size_t condition_num;           // Length of conditions in Where clause
  Condition conditions[MAX_NUM];  // conditions in Where clause
} Deletes;

// struct of update
typedef struct {
  char *relation_name;            // Relation to update
  /* char *attribute_name;           // Attribute to update */
  /* Value value;                    // update value */
  size_t condition_num;           // Length of conditions in Where clause
  Condition conditions[MAX_NUM];  // conditions in Where clause
  char *attributes[MAX_NUM];
  Value values[MAX_NUM];
  int attribute_num;
} Updates;

typedef struct {
  char *name;     // Attribute name
  AttrType type;  // Type of attribute
  size_t length;  // Length of attribute
  int nullable;
} AttrInfo;

// struct of craete_table
typedef struct {
  char *relation_name;           // Relation name
  size_t attribute_count;        // Length of attribute
  AttrInfo attributes[MAX_NUM];  // attributes
} CreateTable;

// struct of drop_table
typedef struct {
  char *relation_name;  // Relation name
} DropTable;

// struct of create_index
typedef struct {
  char *index_name;      // Index name
  char *relation_name;   // Relation name
  char *attribute_names[MAX_NUM];  // Attribute name
  int attribute_num;
  int unique;
} CreateIndex;

// struct of  drop_index
typedef struct {
  const char *index_name;  // Index name
} DropIndex;

typedef struct {
  const char *relation_name;
} DescTable;

typedef struct {
  const char *relation_name;
  const char *file_name;
} LoadData;

typedef struct {
  const char *relation_name;
} ShowIndex;

union Queries {
  Selects *selection;
  Inserts insertion;
  Deletes deletion;
  Updates update;
  CreateTable create_table;
  DropTable drop_table;
  CreateIndex create_index;
  DropIndex drop_index;
  DescTable desc_table;
  LoadData load_data;
  char *errors;
  ShowIndex show_index;
};

// 修改yacc中相关数字编码为宏定义
enum SqlCommandFlag {
  SCF_ERROR = 0,
  SCF_SELECT,
  SCF_INSERT,
  SCF_UPDATE,
  SCF_DELETE,
  SCF_CREATE_TABLE,
  SCF_DROP_TABLE,
  SCF_SHOW_INDEX,
  SCF_CREATE_INDEX,
  SCF_DROP_INDEX,
  SCF_SYNC,
  SCF_SHOW_TABLES,
  SCF_DESC_TABLE,
  SCF_BEGIN,
  SCF_COMMIT,
  SCF_CLOG_SYNC,
  SCF_ROLLBACK,
  SCF_LOAD_DATA,
  SCF_HELP,
  SCF_EXIT
};

// struct of flag and sql_struct
typedef struct Query {
  enum SqlCommandFlag flag;
  union Queries sstr;
  Selects selects[MAX_NUM];
  int selects_num;
  ValueList valuelists[MAX_NUM];
  int valuelist_num;
} Query;

typedef struct {
  ExpType type;
  int l_brace;
  int r_brace;
  Value value;
} ast_val;

typedef struct {
  ExpType type;
  int l_brace;
  int r_brace;
  RelAttr attr;
} ast_attr;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

void relation_attr_init(RelAttr *relation_attr, const char *relation_name, const char *attribute_name);
void relation_attr_destroy(RelAttr *relation_attr);

void aggregation_attr_init(RelAttr *relation_attr, const char *relation_name, const char *attribute_name, AggreType type, int is_digit);
void aggregation_attr_init_expression(RelAttr *relation_attr, const char *relation_name, const char *attribute_name,
    AggreType type, int is_digit, ast *a);

void value_init_integer(Value *value, int v);
void value_init_null(Value *value);
void value_init_float(Value *value, float v);
void value_init_string(Value *value, const char *v);
void value_init_date(Value *value, const char* v);
void value_init_select(Value *value, Selects *selects);
void value_init_list(Value *value, ValueList *valuelist);
void value_destroy(Value *value);

void condition_init(Condition *condition, CompOp comp, int left_is_attr, RelAttr *left_attr, Value *left_value,
    int right_is_attr, RelAttr *right_attr, Value *right_value);
void condition_destroy(Condition *condition);

void attr_info_init(AttrInfo *attr_info, const char *name, AttrType type, size_t length, int nullable);
// void attr_info_init(AttrInfo *attr_info, const char *name, AttrType type, size_t length);
void attr_info_destroy(AttrInfo *attr_info);

void selects_init(Selects *selects, ...);
void selects_reverse_relations(Selects *selects, int len);
void selects_append_in_value(ValueList *valuelist, Value *value);
void selects_append_attribute(Selects *selects, RelAttr *rel_attr);
void selects_append_attribute_expression(Selects *selects, ast *a);
void selects_append_groupby(Selects *selects, RelAttr *groupby_attr);
void selects_append_relation(Selects *selects, const char *relation_name);
void selects_append_order_field(Selects* selects, RelAttr* attr, size_t is_desc);
void selects_append_conditions(Selects* selects,
                               Condition conditions[],
                               size_t condition_num);
void selects_append_having_conditions(Selects* selects,
                                      Condition having_conditions[],
                                      size_t having_condition_num);
void selects_destroy(Selects *selects);

//void inserts_init(Inserts *inserts, const char *relation_name, Value values[], size_t value_num);
void inserts_init(Inserts *inserts, const char *relation_name);
void inserts_append_values(Inserts *inserts, Value values[], size_t value_num);
void inserts_destroy(Inserts *inserts);

void deletes_init_relation(Deletes *deletes, const char *relation_name);
void deletes_set_conditions(Deletes *deletes, Condition conditions[], size_t condition_num);
void deletes_destroy(Deletes *deletes);

void updates_init(Updates *updates, const char *relation_name, Condition conditions[], size_t condition_num);
void updates_destroy(Updates *updates);
void updates_append(Updates *updates, const char *attribute_name, Value *value);

void create_table_append_attribute(CreateTable *create_table, AttrInfo *attr_info);
void create_table_init_name(CreateTable *create_table, const char *relation_name);
void create_table_destroy(CreateTable *create_table);

void drop_table_init(DropTable *drop_table, const char *relation_name);
void drop_table_destroy(DropTable *drop_table);

void create_index_init(
CreateIndex *create_index, const char *index_name, const char *relation_name, int unique);
void show_index_init(ShowIndex *show_index, const char *relation_name);
void create_index_destroy(CreateIndex *create_index);
void create_index_append(CreateIndex *create_index, const char *attribute_name);

void drop_index_init(DropIndex *drop_index, const char *index_name);
void drop_index_destroy(DropIndex *drop_index);

void desc_table_init(DescTable *desc_table, const char *relation_name);
void desc_table_destroy(DescTable *desc_table);

void load_data_init(LoadData *load_data, const char *relation_name, const char *file_name);
void load_data_destroy(LoadData *load_data);

void query_init(Query *query);
Query *query_create();  // create and init
void query_reset(Query *query);
void query_destroy(Query *query);  // reset and delete

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // __OBSERVER_SQL_PARSER_PARSE_DEFS_H__