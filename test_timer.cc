#include <iostream>
#include <set>
#include "server.h"
#include "timer.h"
#include "timestamp.h"
#include "io_context.h"

void print(std::string& str)
{
  std::cout << str << std::endl;
}

void printf_time()
{
  std::cout << timestamp::now().to_string() << std::endl;
}

struct Foo
{
  void print()
  {
    std::cout << "foo" << std::endl;
  }
  static void print_s()
  {
    std::cout << "foo static func" << std::endl;
  }
};

int main(void)
{
  io_context context;

  timer t1(&context);
  timer t2(&context);
  timer t3(&context);
  timer t4(&context);
  timer t5(&context);
  timer t6(&context);
  timer t7(&context);

  // timer s1 = t1;
  // timer s2(&context);
  // s2 = t2;

  t1.set(3, std::bind(print, std::string("one")), false);
  t2.set(3, std::bind(print, std::string("two")), false);
  t3.set(3, std::bind(print, std::string("three")), false);
  Foo f;
  t4.set(10, printf_time, false);
  t5.set(4, std::bind(&Foo::print, &f), false);
  t6.set(5, Foo::print_s, false);
  t7.set(3, []()
  {
    std::cout << "call lambda" << std::endl;
  }, true);

  context.add_func(std::bind(print, std::string("this is once call")));

  context.run();
}
