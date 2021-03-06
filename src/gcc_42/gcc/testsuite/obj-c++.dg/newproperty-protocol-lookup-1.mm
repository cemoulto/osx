/* APPLE LOCAL file radar 4959107 */

#include <Foundation/Foundation.h>
/* { dg-options "-mmacosx-version-min=10.5 -fobjc-new-property -framework Foundation" } */
/* { dg-do run { target *-*-darwin* } } */

@protocol PropProt
@property(readwrite) int foo;
@end

@interface MyClass : NSObject <PropProt>
{
    int _foo;
}
@property(readwrite) int foo;
@end


@implementation MyClass

@synthesize foo = _foo;

@end

int test1(MyClass* testObj)
{
    if (testObj.foo != 5)
      abort ();
    return testObj.foo;
}

int test2(NSObject<PropProt>* testObj)
{
    if (testObj.foo != 3)
      abort ();
    return [testObj foo];
}

int test3(NSObject<PropProt>* testObj)
{
    if (testObj.foo != 99)
      abort ();
    return testObj.foo;
}

int main(void)
{
    MyClass *testObj = [MyClass new];
    testObj.foo = 5;
    test1(testObj);
    testObj.foo = 3;
    test2(testObj);
    testObj.foo = 99;
    test3(testObj);
    return 0;
}
