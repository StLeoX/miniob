
%{

#include "sql/parser/parse_defs.h"
#include "sql/parser/yacc_sql.tab.h"
#include "sql/parser/lex.yy.h"
#ifdef YYDEBUG
  int yydebug = 1;
#endif
// #include "common/log/log.h" // 包含C++中的头文件

#include<stdio.h>
#include<stdlib.h>
#include<string.h>

typedef struct ParserContext {
  Query * ssql;
  // size_t select_length;
  // size_t from_length;
  size_t update_length;

  Value values[MAX_NUM][MAX_NUM];
  size_t value_lengths[MAX_NUM];

  Condition conditions[MAX_NUM][MAX_NUM];
  size_t condition_lengths[MAX_NUM];
  Condition having_conditions[MAX_NUM][MAX_NUM];
  size_t having_condition_lengths[MAX_NUM];

  CompOp comps[MAX_NUM];
  char id[MAX_NUM];
  size_t is_desc;

  size_t joins;

  size_t valuelist_length;
  size_t in_valuelist_num;

  // select start from 1;
  size_t selects_num;
  // int selects_stack[MAX_NUM];
  // int selects_ptr;

  int stack[MAX_NUM];
  int ptr;

  AggreType a_types[MAX_NUM];
  RelAttr aggr_attrs[MAX_NUM][MAX_NUM];
  size_t aggr_attr_lens[MAX_NUM];
  // for aggrs
  int in_having;

  Selects left_sub_select;
  Selects right_sub_select;

} ParserContext;

//获取子串
char *substr(const char *s,int n1,int n2)/*从s中提取下标为n1~n2的字符组成一个新字符串，然后返回这个新串的首地址*/
{
  char *sp = malloc(sizeof(char) * (n2 - n1 + 2));
  int i, j = 0;
  for (i = n1; i <= n2; i++) {
    sp[j++] = s[i];
  }
  sp[j] = 0;
  return sp;
}

void yyerror(yyscan_t scanner, const char *str)
{
  ParserContext *context = (ParserContext *)(yyget_extra(scanner));
  query_reset(context->ssql);
  context->ssql->flag = SCF_ERROR;
  // context->condition_length = 0;
  // context->from_length = 0;
  // context->select_length = 0;
  context->update_length = 0;
  memset(context->value_lengths, 0, sizeof(context->value_lengths));
  memset(context->condition_lengths, 0, sizeof(context->condition_lengths));
  memset(context->having_condition_lengths, 0, sizeof(context->having_condition_lengths));
  memset(context->a_types, 0, sizeof(context->a_types));
  memset(context->aggr_attr_lens, 0, sizeof(context->aggr_attr_lens));
  context->ssql->sstr.insertion.valuelist_num = 0;
  context->selects_num = 0;
  context->joins = 0;
  context->in_valuelist_num = 0;
  context->ptr = 0;
  printf("[YACC] parse sql failed. error=%s", str);
}

ParserContext *get_context(yyscan_t scanner)
{
  return (ParserContext *)yyget_extra(scanner);
}

#define CONTEXT get_context(scanner)
#define S_TOP (CONTEXT->stack[CONTEXT->ptr])

%}

%define api.pure full
%lex-param { yyscan_t scanner }
%parse-param { void *scanner }

//标识tokens
%token  SEMICOLON
        CREATE
        DROP
        TABLE
        TABLES
        INDEX
        SELECT
        DESC
        SHOW
        SYNC
        INSERT
        DELETE
        UPDATE
        LBRACE
        RBRACE
        COMMA
        TRX_BEGIN
        TRX_COMMIT
        TRX_ROLLBACK
        INT_T
        STRING_T
        FLOAT_T
        HELP
        EXIT
        DOT //QUOTE
        INTO
        VALUES
        FROM
        WHERE
        AND
        SET
        ON
        LOAD
        DATA_T
        INFILE
        EQ
        IS
        LT
        GT
        LE
        GE
        NE
        DATE_T
        UNIQUE_T
        MAX
        MIN
        COUNT
        AVG
        SUM
        NULLABLE
        NULL_V
        NOT
        INNER
        JOIN
        LIKE
        ORDER
        GROUP
        ASC
        BY
        TEXT_T
        IN
        EXISTS
        HAVING
        ADD
        SUB
        DIV

%union {
  struct _Attr *attr;
  struct _Condition *condition1;
  struct _Value *value1;
  char *string;
  int number;
  float floats;
  char *position;
  struct ast *ast1;
}

%left ADD SUB
%left STAR DIV

// 终结符
%token <number> NUMBER
%token <string> DATE_STR;
%token <floats> FLOAT
%token <string> ID
%token <string> PATH
%token <string> SSS
%token <string> STAR
%token <string> STRING_V

//非终结符
%type <number> type;
%type <condition1> condition;
%type <value1> value;
%type <number> number;
%type <ast1> exp

%%

commands:		//commands or sqls. parser starts here.
    /* empty */
    | commands command
;

command:
	  select
	| insert
	| update
	| delete
	| create_table
	| drop_table
	| show_tables
	| desc_table
	| create_index
	| drop_index
	| sync
	| begin
	| commit
	| rollback
	| load_data
	| help
	| exit
  | show_index
;

exit:
  EXIT SEMICOLON {
      CONTEXT->ssql->flag=SCF_EXIT;//"exit";
  };

help:
  HELP SEMICOLON {
      CONTEXT->ssql->flag=SCF_HELP;//"help";
  };

sync:
  SYNC SEMICOLON {
    CONTEXT->ssql->flag = SCF_SYNC;
  };

begin:
  TRX_BEGIN SEMICOLON {
    CONTEXT->ssql->flag = SCF_BEGIN;
  };

commit:
  TRX_COMMIT SEMICOLON {
    CONTEXT->ssql->flag = SCF_COMMIT;
  };

rollback:
  TRX_ROLLBACK SEMICOLON {
    CONTEXT->ssql->flag = SCF_ROLLBACK;
  };

drop_table:
  DROP TABLE ID SEMICOLON {
      CONTEXT->ssql->flag = SCF_DROP_TABLE;//"drop_table";
      drop_table_init(&CONTEXT->ssql->sstr.drop_table, $3);
  };

show_tables:
  SHOW TABLES SEMICOLON {
    CONTEXT->ssql->flag = SCF_SHOW_TABLES;
  };

desc_table:
  DESC ID SEMICOLON {
    CONTEXT->ssql->flag = SCF_DESC_TABLE;
    desc_table_init(&CONTEXT->ssql->sstr.desc_table, $2);
  };

show_index:
  SHOW INDEX FROM ID SEMICOLON {
    CONTEXT->ssql->flag = SCF_SHOW_INDEX;
    show_index_init(&CONTEXT->ssql->sstr.show_index, $4);
  };

create_index:
  CREATE INDEX ID ON ID LBRACE ID index_ids RBRACE SEMICOLON {
    CONTEXT->ssql->flag = SCF_CREATE_INDEX;//"create_index";
    create_index_append(&CONTEXT->ssql->sstr.create_index, $7);
    create_index_init(&CONTEXT->ssql->sstr.create_index, $3, $5, 0);
  }
  | CREATE UNIQUE_T INDEX ID ON ID LBRACE ID index_ids RBRACE SEMICOLON {
    CONTEXT->ssql->flag = SCF_CREATE_INDEX;//"create_index";
    create_index_append(&CONTEXT->ssql->sstr.create_index, $8);
    create_index_init(&CONTEXT->ssql->sstr.create_index, $4, $6, 1);
  };

index_ids:
  /* empty */
  | COMMA ID index_ids {
    create_index_append(&CONTEXT->ssql->sstr.create_index, $2);
  }
;

drop_index:
  DROP INDEX ID  SEMICOLON {
    CONTEXT->ssql->flag=SCF_DROP_INDEX;//"drop_index";
    drop_index_init(&CONTEXT->ssql->sstr.drop_index, $3);
  };

create_table:
  CREATE TABLE ID LBRACE attr_def attr_def_list RBRACE SEMICOLON {
    CONTEXT->ssql->flag=SCF_CREATE_TABLE;//"create_table";
    // CONTEXT->ssql->sstr.create_table.attribute_count = CONTEXT->value_length;
    create_table_init_name(&CONTEXT->ssql->sstr.create_table, $3);
    //临时变量清零
    CONTEXT->value_lengths[S_TOP] = 0;
  };

attr_def_list:
  /* empty */
  | COMMA attr_def attr_def_list {    }
  ;

attr_def:
  ID_ext type LBRACE number RBRACE {
    AttrInfo attribute;
    attr_info_init(&attribute, CONTEXT->id, $2, $4, 0);
    create_table_append_attribute(&CONTEXT->ssql->sstr.create_table, &attribute);
    // CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].name =(char*)malloc(sizeof(char));
    // strcpy(CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].name, CONTEXT->id);
    // CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].type = $2;
    // CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].length = $4;
    CONTEXT->value_lengths[S_TOP]++;
  }
  | ID_ext type {
    AttrInfo attribute;
    if ($2 == TEXTS) {
      // TODO
      attr_info_init(&attribute, CONTEXT->id, $2, 4096,  0);
    } else {
      attr_info_init(&attribute, CONTEXT->id, $2, 4,  0);
    }
    create_table_append_attribute(&CONTEXT->ssql->sstr.create_table, &attribute);
    // CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].name=(char*)malloc(sizeof(char));
    // strcpy(CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].name, CONTEXT->id);
    // CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].type=$2;
    // CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].length=4; // default attribute length
    CONTEXT->value_lengths[S_TOP]++;
  }
  | ID_ext type NULLABLE {
      AttrInfo attribute;
      attr_info_init(&attribute, CONTEXT->id, $2, 4, 1);
      create_table_append_attribute(&CONTEXT->ssql->sstr.create_table, &attribute);
      CONTEXT->value_lengths[S_TOP]++;
  }
  | ID_ext type NOT NULL_V {
        AttrInfo attribute;
        attr_info_init(&attribute, CONTEXT->id, $2, 4, 0);
        create_table_append_attribute(&CONTEXT->ssql->sstr.create_table, &attribute);
        CONTEXT->value_lengths[S_TOP]++;
  }
;

number:
		NUMBER {$$ = $1;}
;

type:
	INT_T { $$=INTS; }
       | STRING_T { $$=CHARS; }
       | FLOAT_T { $$=FLOATS; }
       | DATE_T { $$=DATES; }
       | TEXT_T { $$=TEXTS; }
;

ID_ext:
	ID {
		char *temp=$1;
		snprintf(CONTEXT->id, sizeof(CONTEXT->id), "%s", temp);
	};


insert:				/*insert   语句的语法解析树*/
INSERT INTO ID VALUES insert_brace insert_element insert_brace SEMICOLON {
  // CONTEXT->values[CONTEXT->value_length++] = *$6;
  CONTEXT->ssql->flag=SCF_INSERT;//"insert";
  inserts_init(&CONTEXT->ssql->sstr.insertion, $3);
  //inserts_init(&CONTEXT->ssql->sstr.insertion, $3, CONTEXT->values, CONTEXT->value_length);
  //临时变量清零
  CONTEXT->value_lengths[S_TOP]=0;
  };

insert_brace:
  | COMMA insert_element insert_brace {
  };

insert_element:
LBRACE value value_list RBRACE {
  inserts_append_values(&CONTEXT->ssql->sstr.insertion, CONTEXT->values[S_TOP], CONTEXT->value_lengths[S_TOP]);
  CONTEXT->value_lengths[S_TOP] = 0;
  };

value_list:
  /* empty */
  | COMMA value value_list  {
    // CONTEXT->values[CONTEXT->value_length++] = *$2;
  };

value:
  NUMBER{
    value_init_integer(&CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP]++], $1);
    // value_init_integer(&CONTEXT->values[CONTEXT->value_length++], $1);
  }
  | FLOAT{
    value_init_float(&CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP]++], $1);
  }
  | SSS {
      $1 = substr($1,1,strlen($1)-2);
    value_init_string(&CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP]++], $1);
  }
  | DATE_STR {
      value_init_date(&CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP]++], $1);
  }
  | NULL_V {
        value_init_null(&CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP]++]);
  }
  | LBRACE select_ RBRACE {
        value_init_select(&CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP]++],
                      &CONTEXT->ssql->selects[CONTEXT->stack[CONTEXT->ptr+1]]);
  }
;

exp:
	exp ADD exp { $$ = newast(ADDN, $1, $3); }
	| SUB exp { $$ = newast(SUBN, NULL, $2); }
	| exp SUB exp { $$ = newast(SUBN, $1, $3); }
	| exp STAR exp { $$ = newast(MULN, $1, $3); }
	| exp DIV exp { $$ = newast(DIVN, $1, $3); }
	| LBRACE exp RBRACE {
		ast *a = (ast *)$2;
		a->l_brace++;
		a->r_brace++;
		$$ = $2;
	}
	| value { $$ = newvalNode($1); }
	| ID {
		RelAttr attr;
		relation_attr_init(&attr, NULL, $1);
		$$ = newattrNode(&attr);
	}
	| ID DOT ID {
		RelAttr attr;
		relation_attr_init(&attr, $1, $3);
		$$ = newattrNode(&attr);
	}
	| ID DOT STAR {
		RelAttr attr;
		relation_attr_init(&attr, $1, "*");
		$$ = newattrNode(&attr);
	}
;

delete:
  DELETE FROM ID where SEMICOLON
  {
    CONTEXT->ssql->flag = SCF_DELETE;//"delete";
    deletes_init_relation(&CONTEXT->ssql->sstr.deletion, $3);
    deletes_set_conditions(&CONTEXT->ssql->sstr.deletion,
        CONTEXT->conditions[S_TOP], CONTEXT->condition_lengths[S_TOP]);
    CONTEXT->condition_lengths[S_TOP] = 0;
  }
;

update:
  UPDATE ID SET update_set updates_sets where SEMICOLON {
    CONTEXT->ssql->flag = SCF_UPDATE;//"update";
    updates_init(&CONTEXT->ssql->sstr.update, $2,
                 CONTEXT->conditions[S_TOP], CONTEXT->condition_lengths[S_TOP]);
    CONTEXT->condition_lengths[S_TOP] = 0;
    CONTEXT->value_lengths[S_TOP] = 0;
  }
;

update_set:
  ID EQ value {
    // updates_append(&CONTEXT->ssql->sstr.update, $1, &CONTEXT->values[CONTEXT->value_lengths[S_TOP] - 1]);
    updates_append(&CONTEXT->ssql->sstr.update, $1, &CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP] - 1]);
  }
;

updates_sets:
  /* empty */
  | COMMA update_set updates_sets {
  }
;

select_stmt:
  SELECT {
    CONTEXT->stack[++CONTEXT->ptr] = ++CONTEXT->selects_num;
    // printf("push select %zu, ptr: %d\n", CONTEXT->selects_num, CONTEXT->ptr);
    // CONTEXT->selects_num++;
    CONTEXT->ssql->selects_num = CONTEXT->selects_num;
  }
;

select_:
  select_stmt select_attr FROM rel_name rel_list where groupby having order {
    // printf("SELECT: num: %d, ptr: %d pop, table: %s, cond num: %d\n", S_TOP, CONTEXT->ptr, CONTEXT->ssql->selects[S_TOP].relations[0], CONTEXT->condition_lengths[S_TOP]);
    selects_append_conditions(&CONTEXT->ssql->selects[S_TOP], CONTEXT->conditions[S_TOP], CONTEXT->condition_lengths[S_TOP]);
    selects_append_having_conditions(&CONTEXT->ssql->selects[S_TOP], CONTEXT->having_conditions[S_TOP], CONTEXT->having_condition_lengths[S_TOP]);
    CONTEXT->ptr--;
  }
;

select:
  select_ SEMICOLON {
    // int num = CONTEXT->selects_num;
    // printf("SELECT: num: %d, ptr: %d pop, table: %s, cond num: %d", S_TOP, CONTEXT->ptr, CONTEXT->ssql->selects[S_TOP].relations[0], CONTEXT->condition_lengths[S_TOP]);
    CONTEXT->ssql->sstr.selection = &CONTEXT->ssql->selects[1];
    CONTEXT->ssql->flag=SCF_SELECT;//"select";

    //临时变量清零
    CONTEXT->condition_lengths[S_TOP]=0;
    // CONTEXT->from_length=0;
    // CONTEXT->select_length=0;
    CONTEXT->value_lengths[S_TOP] = 0;
  }
;

having:
  /* empty */
  | HAVING condition condition_list {

  }
;

groupby:
  /* empty */
  | GROUP BY ID groupby_ids {
    RelAttr attr;
    relation_attr_init(&attr, NULL, $3);
    selects_append_groupby(&CONTEXT->ssql->selects[S_TOP], &attr);
  }
  | GROUP BY ID DOT ID groupby_ids {
    RelAttr attr;
    relation_attr_init(&attr, $3, $5);
    selects_append_groupby(&CONTEXT->ssql->selects[S_TOP], &attr);
  }
;

groupby_ids:
  /* empty */
  | COMMA ID groupby_ids {
    RelAttr attr;
    relation_attr_init(&attr, NULL, $2);
    selects_append_groupby(&CONTEXT->ssql->selects[S_TOP], &attr);
  }
  | COMMA ID DOT ID groupby_ids {
    RelAttr attr;
    relation_attr_init(&attr, $2, $4);
    selects_append_groupby(&CONTEXT->ssql->selects[S_TOP], &attr);
  }
;

select_attr:
  STAR attr_list {
    RelAttr attr;
    relation_attr_init(&attr, NULL, "*");
    selects_append_attribute(&CONTEXT->ssql->selects[S_TOP], &attr);
  }
  | ID attr_list {
    RelAttr attr;
    relation_attr_init(&attr, NULL, $1);
    selects_append_attribute(&CONTEXT->ssql->selects[S_TOP], &attr);
  }
  | ID DOT ID attr_list {
    RelAttr attr;
    relation_attr_init(&attr, $1, $3);
    selects_append_attribute(&CONTEXT->ssql->selects[S_TOP], &attr);
  }
  // | exp attr_list {
  //   selects_append_attribute_expression(&CONTEXT->selects[CONTEXT->select_length-1], $1);
  // }
  | aggregation_attr attr_list {
    // printf("[YACC] append aggr\n");
    selects_append_attribute(&CONTEXT->ssql->selects[S_TOP], &CONTEXT->aggr_attrs[S_TOP][--CONTEXT->aggr_attr_lens[S_TOP]]);
  }
;

aggregation_attr:
  aggregation_func LBRACE ID aggregation_extra_id RBRACE {
    aggregation_attr_init(&CONTEXT->aggr_attrs[S_TOP][CONTEXT->aggr_attr_lens[S_TOP]++], NULL, $3, CONTEXT->a_types[S_TOP], 0);
    CONTEXT->a_types[S_TOP] = A_NO;
  }
  | aggregation_func LBRACE ID DOT ID aggregation_extra_id RBRACE {
    aggregation_attr_init(&CONTEXT->aggr_attrs[S_TOP][CONTEXT->aggr_attr_lens[S_TOP]++], $3, $5, CONTEXT->a_types[S_TOP], 0);
    // selects_append_attribute(&CONTEXT->ssql->selects[S_TOP], &attr);
    CONTEXT->a_types[S_TOP] = A_NO;
  }
  // | aggregation_func LBRACE exp aggregation_extra_id RBRACE {
  //   aggregation_attr_init_expression(&CONTEXT->aggr_attrs[S_TOP][CONTEXT->aggr_attr_lens[S_TOP]++], $3, $5, CONTEXT->a_types[S_TOP], 0);
  //   CONTEXT->a_types[S_TOP] = A_NO;
  // }
  | aggregation_func LBRACE RBRACE {
    aggregation_attr_init(&CONTEXT->aggr_attrs[S_TOP][CONTEXT->aggr_attr_lens[S_TOP]++], NULL, "fail", A_FAILURE, 0);
    // selects_append_attribute(&CONTEXT->ssql->selects[S_TOP], &attr);
    CONTEXT->a_types[S_TOP] = A_NO;
  }
  | aggregation_func LBRACE STAR aggregation_extra_id RBRACE {
    aggregation_attr_init(&CONTEXT->aggr_attrs[S_TOP][CONTEXT->aggr_attr_lens[S_TOP]++], NULL, "*", CONTEXT->a_types[S_TOP] != A_COUNT ? A_FAILURE : CONTEXT->a_types[S_TOP], 1);
    // selects_append_attribute(&CONTEXT->ssql->selects[S_TOP], &attr);
    CONTEXT->a_types[S_TOP] = A_NO;
  }
  | aggregation_func LBRACE NUMBER aggregation_extra_id RBRACE {
    char *str = malloc(10 * sizeof(char));
    snprintf(str, 10, "%d", $3);
    aggregation_attr_init(&CONTEXT->aggr_attrs[S_TOP][CONTEXT->aggr_attr_lens[S_TOP]++], NULL, str, CONTEXT->a_types[S_TOP], 1);
    // selects_append_attribute(&CONTEXT->ssql->selects[S_TOP], &attr);
    CONTEXT->a_types[S_TOP] = A_NO;
  }
  | aggregation_func LBRACE FLOAT aggregation_extra_id RBRACE {
    char *buf = malloc(20 * sizeof(char));
    snprintf(buf, 20, "%.2f", $3);
    size_t len = strlen(buf);
    while (buf[len - 1] == '0') {
      len--;
    }
    if (buf[len - 1] == '.') {
      len--;
    }
    buf[len] = '\0';
    aggregation_attr_init(&CONTEXT->aggr_attrs[S_TOP][CONTEXT->aggr_attr_lens[S_TOP]++], NULL, buf, CONTEXT->a_types[S_TOP], 1);
    // selects_append_attribute(&CONTEXT->ssql->selects[S_TOP], &attr);
    CONTEXT->a_types[S_TOP] = A_NO;
  }
;

aggregation_func:
  MAX {
    CONTEXT->a_types[S_TOP] = A_MAX;
  }
  | MIN {
    CONTEXT->a_types[S_TOP] = A_MIN;
  }
  | AVG {
    CONTEXT->a_types[S_TOP] = A_AVG;
  }
  | COUNT {
    CONTEXT->a_types[S_TOP] = A_COUNT;
  }
  | SUM {
    CONTEXT->a_types[S_TOP] = A_SUM;
  }
;

aggregation_extra_id:
  /* empty */
  | COMMA ID aggregation_extra_id {
    CONTEXT->a_types[S_TOP] = A_FAILURE;
  }
  | COMMA ID DOT aggregation_extra_id {
    CONTEXT->a_types[S_TOP] = A_FAILURE;
  }
  | COMMA STAR aggregation_extra_id {
    CONTEXT->a_types[S_TOP] = A_FAILURE;
  }
  | COMMA NUMBER aggregation_extra_id {
    CONTEXT->a_types[S_TOP] = A_FAILURE;
  }
;

attr_list:
  /* empty */
  // | COMMA exp attr_list {
  //       selects_append_attribute_expression(&CONTEXT->selects[CONTEXT->select_length-1], $2);
  // }
  | COMMA ID attr_list {
    RelAttr attr;
    relation_attr_init(&attr, NULL, $2);
    selects_append_attribute(&CONTEXT->ssql->selects[S_TOP], &attr);
      // CONTEXT->ssql->sstr.selection.attributes[CONTEXT->select_length].relation_name = NULL;
      // CONTEXT->ssql->sstr.selection.attributes[CONTEXT->select_length++].attribute_name=$2;
  }
  | COMMA ID DOT ID attr_list {
    RelAttr attr;
    relation_attr_init(&attr, $2, $4);
    selects_append_attribute(&CONTEXT->ssql->selects[S_TOP], &attr);
      // CONTEXT->ssql->sstr.selection.attributes[CONTEXT->select_length].attribute_name=$4;
      // CONTEXT->ssql->sstr.selection.attributes[CONTEXT->select_length++].relation_name=$2;
  }
  | COMMA STAR attr_list {
    RelAttr attr;
    relation_attr_init(&attr, NULL, "*");
    selects_append_attribute(&CONTEXT->ssql->selects[S_TOP], &attr);
  }
  | COMMA aggregation_attr attr_list {
    // printf("append aggr\n");
    selects_append_attribute(&CONTEXT->ssql->selects[S_TOP], &CONTEXT->aggr_attrs[S_TOP][--CONTEXT->aggr_attr_lens[S_TOP]]);
  }
;

rel_name:
  ID inner_joins {
    selects_append_relation(&CONTEXT->ssql->selects[S_TOP], $1);
    selects_reverse_relations(&CONTEXT->ssql->selects[S_TOP], ++CONTEXT->joins);
    CONTEXT->joins = 0;
  }
;

inner_joins:
  /* empty */
  | INNER JOIN ID ON condition condition_list inner_joins {
    selects_append_relation(&CONTEXT->ssql->selects[S_TOP], $3);
    CONTEXT->joins++;
  }
;

order: 
  /* empty */
  | ORDER BY order_component order_component_list {
  }
;

order_component:
  | ID order_direction {
      RelAttr attr;
      relation_attr_init(&attr, NULL, $1);
      selects_append_order_field(&CONTEXT->ssql->selects[S_TOP], &attr, CONTEXT->is_desc);
      CONTEXT->is_desc = 0;
  }
  | ID DOT ID order_direction {
      RelAttr attr;
      relation_attr_init(&attr, $1, $3);
      selects_append_order_field(&CONTEXT->ssql->selects[S_TOP], &attr, CONTEXT->is_desc);
      CONTEXT->is_desc = 0;
  }
;

order_direction: 
  ASC {
  CONTEXT->is_desc = 0;
  }
  | DESC {
  CONTEXT->is_desc = 1;
  }
  | {
  CONTEXT->is_desc = 0;
  }
;

order_component_list:
  /* empty */
  | COMMA order_component order_component_list {
    // nothing
  }
;

rel_list:
  /* empty */
  | COMMA rel_name rel_list {
    // selects_append_relation(&CONTEXT->ssql->selects[CONTEXT->selects_num], $2);
  }
;

where:
  /* empty */
  | WHERE condition condition_list {
    // CONTEXT->conditions[CONTEXT->condition_length++]=*$2;
  }
;

condition_list:
  /* empty */
  | AND condition condition_list {
      // CONTEXT->conditions[CONTEXT->condition_length++]=*$2;
  }
;

condition:
  ID comOp value {
    RelAttr left_attr;
    relation_attr_init(&left_attr, NULL, $1);
    // printf("current ptr %d\n", CONTEXT->ptr);
    Value *right_value = &CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP] - 1];
    Condition condition;
    condition_init(&condition, CONTEXT->comps[S_TOP], 1, &left_attr, NULL, 0, NULL, right_value);
    CONTEXT->conditions[S_TOP][CONTEXT->condition_lengths[S_TOP]++] = condition;
  }
  | value comOp ID {
    Value *left_value = &CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP] - 1];
    RelAttr right_attr;
    relation_attr_init(&right_attr, NULL, $3);
    Condition condition;
    condition_init(&condition, CONTEXT->comps[S_TOP], 0, NULL, left_value, 1, &right_attr, NULL);
    CONTEXT->conditions[S_TOP][CONTEXT->condition_lengths[S_TOP]++] = condition;
  }
  | value comOp value {
    Value *left_value = &CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP] - 2];
    Value *right_value = &CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP] - 1];
    Condition condition;
    condition_init(&condition, CONTEXT->comps[S_TOP], 0, NULL, left_value, 0, NULL, right_value);
    CONTEXT->conditions[S_TOP][CONTEXT->condition_lengths[S_TOP]++] = condition;
  }
  | ID comOp ID {
    RelAttr left_attr;
    relation_attr_init(&left_attr, NULL, $1);
    RelAttr right_attr;
    relation_attr_init(&right_attr, NULL, $3);
    Condition condition;
    condition_init(&condition, CONTEXT->comps[S_TOP], 1, &left_attr, NULL, 1, &right_attr, NULL);
    CONTEXT->conditions[S_TOP][CONTEXT->condition_lengths[S_TOP]++] = condition;
  }
  | ID DOT ID comOp value {
    RelAttr left_attr;
    relation_attr_init(&left_attr, $1, $3);
    Value *right_value = &CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP] - 1];
    Condition condition;
    condition_init(&condition, CONTEXT->comps[S_TOP], 1, &left_attr, NULL, 0, NULL, right_value);
          // printf("append condition in %d\n", S_TOP);
    CONTEXT->conditions[S_TOP][CONTEXT->condition_lengths[S_TOP]++] = condition;
  }
  | value comOp ID DOT ID {
    Value *left_value = &CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP] - 1];
    RelAttr right_attr;
    relation_attr_init(&right_attr, $3, $5);
    Condition condition;
    condition_init(&condition, CONTEXT->comps[S_TOP], 0, NULL, left_value, 1, &right_attr, NULL);
    CONTEXT->conditions[S_TOP][CONTEXT->condition_lengths[S_TOP]++] = condition;
  }
  | ID DOT ID comOp ID DOT ID {
    RelAttr left_attr;
    relation_attr_init(&left_attr, $1, $3);
    RelAttr right_attr;
    relation_attr_init(&right_attr, $5, $7);
    Condition condition;
    condition_init(&condition, CONTEXT->comps[S_TOP], 1, &left_attr, NULL, 1, &right_attr, NULL);
    CONTEXT->conditions[S_TOP][CONTEXT->condition_lengths[S_TOP]++] = condition;
  }
  | ID comOp LBRACE in_value in_value_list RBRACE {
    RelAttr left_attr;
    relation_attr_init(&left_attr, NULL, $1);

    value_init_list(&CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP]++],
                    &CONTEXT->ssql->valuelists[CONTEXT->in_valuelist_num]);
    Value *value = &CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP] - 1];

    Condition condition;
    condition_init(&condition, CONTEXT->comps[S_TOP], 1, &left_attr, NULL, 0, NULL, value);
    CONTEXT->conditions[S_TOP][CONTEXT->condition_lengths[S_TOP]++] = condition;
    CONTEXT->in_valuelist_num++;
  }
  | ID DOT ID comOp LBRACE value in_value_list RBRACE {
    RelAttr left_attr;
    relation_attr_init(&left_attr, $1, $3);

    value_init_list(&CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP]++],
                    &CONTEXT->ssql->valuelists[CONTEXT->in_valuelist_num]);
    Value *value = &CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP] - 1];

    Condition condition;
    condition_init(&condition, CONTEXT->comps[S_TOP], 1, &left_attr, NULL, 0, NULL, value);
    CONTEXT->conditions[S_TOP][CONTEXT->condition_lengths[S_TOP]++] = condition;
    CONTEXT->in_valuelist_num++;
  }
  | EXISTS value {
    Value *right_value = &CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP] - 1];
    value_init_null(&CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP]++]);
    Value *left_value = &CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP] - 1];
    Condition condition;
    condition_init(&condition, VALUE_EXISTS, 0, NULL, left_value, 0, NULL, right_value);
    CONTEXT->conditions[S_TOP][CONTEXT->condition_lengths[S_TOP]++] = condition;
  }
  | NOT EXISTS value {
    Value *right_value = &CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP] - 1];
    value_init_null(&CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP]++]);
    Value *left_value = &CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP] - 1];
    Condition condition;
    condition_init(&condition, VALUE_NOT_EXISTS, 0, NULL, left_value, 0, NULL, right_value);
    CONTEXT->conditions[S_TOP][CONTEXT->condition_lengths[S_TOP]++] = condition;
  }
  | aggregation_attr comOp value {
    Value *right_value = &CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP] - 1];
    Condition condition;
    condition_init(&condition, CONTEXT->comps[S_TOP], 1, &CONTEXT->aggr_attrs[S_TOP][--CONTEXT->aggr_attr_lens[S_TOP]], NULL,
                   0, NULL, right_value);
    CONTEXT->having_conditions[S_TOP][CONTEXT->having_condition_lengths[S_TOP]++] = condition;
  }
  | value comOp aggregation_attr {
    Value *left_value = &CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP] - 1];
    Condition condition;
    condition_init(&condition, CONTEXT->comps[S_TOP], 0, NULL, left_value, 1,
                   &CONTEXT->aggr_attrs[S_TOP][--CONTEXT->aggr_attr_lens[S_TOP]], NULL);
    CONTEXT->having_conditions[S_TOP][CONTEXT->having_condition_lengths[S_TOP]++] = condition;
  }
;

in_value:
  value {
    Value *value = &CONTEXT->values[S_TOP][CONTEXT->value_lengths[S_TOP] - 1];
    int num = CONTEXT->in_valuelist_num;

    selects_append_in_value(&CONTEXT->ssql->valuelists[num], value);
  }
;

in_value_list:
  /* empty */
  | COMMA in_value in_value_list {
  }
;

comOp:
  EQ { CONTEXT->comps[S_TOP] = EQUAL_TO; }
  | LT { CONTEXT->comps[S_TOP] = LESS_THAN; }
  | GT { CONTEXT->comps[S_TOP] = GREAT_THAN; }
  | LE { CONTEXT->comps[S_TOP] = LESS_EQUAL; }
  | GE { CONTEXT->comps[S_TOP] = GREAT_EQUAL; }
  | NE { CONTEXT->comps[S_TOP] = NOT_EQUAL; }
  | IS { CONTEXT->comps[S_TOP] = IS_EQUAL; }
  | IS NOT { CONTEXT->comps[S_TOP] = IS_NOT_EQUAL; }
  | NOT LIKE { CONTEXT->comps[S_TOP] = STR_NOT_LIKE; }
  | LIKE { CONTEXT->comps[S_TOP] = STR_LIKE; }
  | IN { CONTEXT->comps[S_TOP] = VALUE_IN; }
  | NOT IN { CONTEXT->comps[S_TOP] = VALUE_NOT_IN; }
;

load_data:
  LOAD DATA_T INFILE SSS INTO TABLE ID SEMICOLON
  {
    CONTEXT->ssql->flag = SCF_LOAD_DATA;
    load_data_init(&CONTEXT->ssql->sstr.load_data, $7, $4);
  }
;

%%
//_____________________________________________________________________
extern void scan_string(const char *str, yyscan_t scanner);

int sql_parse(const char *s, Query *sqls){
	ParserContext context;
	memset(&context, 0, sizeof(context));

	yyscan_t scanner;
	yylex_init_extra(&context, &scanner);
	context.ssql = sqls;
	scan_string(s, scanner);
	int result = yyparse(scanner);
	yylex_destroy(scanner);
	return result;
}
