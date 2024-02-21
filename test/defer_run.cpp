//
// Created by henry on 24-1-23.
//
#include <iostream>
#include <string>
#include "include/defer.h"

using namespace std;

void testFun1(const string& name) { cout << name; }

void testFun2(const string& name) { cout << name; }

int main() {
  cout << "begin..." << endl;
  string str1 = "Hello";
  string str2 = " world";
  DEFER {
    testFun1(str1);
    testFun2(str2);
  };
  cout << "end..." << endl;
  return 0;
}