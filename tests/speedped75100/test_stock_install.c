#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"stock install check failed: %s\n",#x); exit(1); } } while(0)
static bool installed;
static unsigned calls, fail_at;
static bool speedped_stock_installed(void) { return installed; }
static bool step(void) { return ++calls!=fail_at; }
static bool speedped_stock_write_mc(void) { return step(); }
static bool speedped_stock_write_app(void) { return step(); }
static bool speedped_stock_write_bike(void) { return step(); }
static bool speedped_stock_mark_installed(void) { if(!step()) return false; installed=true; return true; }
#include "../../hwconf/SPEEDPED75100/stock_install.h"
int main(void) {
 for(unsigned failure=1;failure<=4;failure++) {
  installed=false; calls=0; fail_at=failure;
  CHECK(!speedped_install_stock_once()); CHECK(!installed && calls==failure);
  calls=0; fail_at=0; CHECK(speedped_install_stock_once()); CHECK(installed && calls==4);
  calls=0; CHECK(speedped_install_stock_once()); CHECK(calls==0);
 }
 puts("Stock install: interrupted stages retry; marker last; later boots preserve edits.");
}
