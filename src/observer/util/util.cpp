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
// Created by wangyunlai on 2022/9/28
//

#include <string.h>
#include <vector>
#include "util/util.h"

std::string double2string(double v)
{
  char buf[256];
  snprintf(buf, sizeof(buf), "%.2f", v);
  size_t len = strlen(buf);
  while (buf[len - 1] == '0') {
    len--;
      
  }
  if (buf[len - 1] == '.') {
    len--;
  }

  return std::string(buf, len);
}

std::string date_to_string(int date)
{
  std::string y = std::to_string(date / 10000);
  std::string m = std::to_string((date % 10000) / 100);
  std::string d = std::to_string(date % 100);
  for (size_t i = 0; i < 4-y.size(); i++) {
    y = '0'+y;
  }
  for (size_t i = 0; i < 2-m.size(); i++) {
    m = '0'+m;
  }
  for (size_t i = 0; i < 2-d.size(); i++) {
    d = '0'+d;
  }
  return y + '-' + m + '-' +d;
}



bool string_like(const char *s1, const char *s2)
{
  int n = strlen(s1), m = strlen(s2);
  auto match = [&](int i, int j) {
    if (i == 0 || j == 0) return false;
    return s1[i-1] == s2[j-1] || s2[j-1] == '_';
  };
  std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1, 0));
  dp[0][0] = true;
  for (int i = 0; i <= n; i++) {
    for (int j = 1; j <= m; j++) {
      if (s2[j-1] == '%') {
        dp[i][j] |= dp[i][j-1];
        if (i > 0) {
          dp[i][j] |= dp[i-1][j];
        }
      } else {
        if (match(i, j)) {
          dp[i][j] = dp[i-1][j-1];
        } else {
          dp[i][j] = false;
        }
      }
    }
  }
  return dp[n][m];
}
