#include <QtCore/QObject>

class A : public QObject {
  Q_OBJECT
public:
  int g;
  A() : g(foo()) {} // OK
  int foo() const { return 5; }
  void bar() const {
    foo();     // WARN
    if (foo()) // OK
    {
    }
  }
};

class B {
public:
  A a;
  B() : g(a.foo()) {} // OK
  int g;
};

int main(int argc, char *argv[]) {
  A a;
  B b;
  a.bar();
  auto lambda = [](A &obj) { obj.foo(); }; // WARN
  lambda(a);
  return 0;
}
