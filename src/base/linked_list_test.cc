#include "linked_list.h"
#include "gtest/gtest.h"

struct TestObject {
  int value;
  LinkedListEntry entry1;
  LinkedListEntry entry2;

  TestObject(int v) : value(v) {}
};

TEST(LinkedListTest, Basic) {
  LINKED_LIST(TestObject, entry1) list;

  EXPECT_TRUE(list.IsEmpty());

  TestObject obj1(1), obj2(2), obj3(3);

  list.PushBack(obj1.entry1);
  EXPECT_FALSE(list.IsEmpty());
  list.PushBack(obj2.entry1);
  EXPECT_FALSE(list.IsEmpty());
  list.PushBack(obj3.entry1);
  EXPECT_FALSE(list.IsEmpty());

  int index = 1;
  for (TestObject& obj : list) {
    EXPECT_EQ(obj.value, index);
    index++;
  }

  obj2.entry1.Remove();
  EXPECT_FALSE(list.IsEmpty());

  decltype(list)::Iter it = list.begin();
  EXPECT_EQ(it->value, 1);
  it++;
  EXPECT_EQ(it->value, 3);
  ++it;
  EXPECT_TRUE(!it);

  list.PushFront(obj2.entry1);
  it = list.begin();
  EXPECT_EQ(it->value, 2);
  it++;
  EXPECT_EQ(it->value, 1);
  it++;
  EXPECT_EQ(it->value, 3);
  ++it;
  EXPECT_TRUE(!it);
}

TEST(LinkedListTest, Pop) {
  LINKED_LIST(TestObject, entry1) list;

  TestObject obj1(1), obj2(2), obj3(3);

  list.PushBack(obj1.entry1);
  list.PushBack(obj2.entry1);
  list.PushBack(obj3.entry1);

  EXPECT_EQ(list.PopFront()->value, 1);
  EXPECT_EQ(list.PopBack()->value, 3);

  decltype(list)::Iter it = list.begin();
  EXPECT_EQ(it->value, 2);
  it++;
  EXPECT_EQ(it, list.end());
}

TEST(LinkedListTest, InteriorInsert) {
  LINKED_LIST(TestObject, entry1) list;

  TestObject obj1(1), obj2(2), obj3(3);

  list.PushBack(obj1.entry1);
  list.PushBack(obj3.entry1);

  obj1.entry1.InsertAfter(obj2.entry1);

  decltype(list)::Iter it = list.begin();
  EXPECT_EQ(it->value, 1);
  it++;
  EXPECT_EQ(it->value, 2);
  it++;
  EXPECT_EQ(it->value, 3);
  ++it;
  EXPECT_EQ(it, list.end());

  obj2.entry1.Remove();
  obj1.entry1.InsertBefore(obj2.entry1);

  it = list.begin();
  EXPECT_EQ(it->value, 2);
  it++;
  EXPECT_EQ(it->value, 1);
  it++;
  EXPECT_EQ(it->value, 3);
  ++it;
  EXPECT_EQ(it, list.end());
}

TEST(LinkedListTest, ReverseIter) {
  LINKED_LIST(TestObject, entry1) list;

  TestObject obj1(1), obj2(2), obj3(3);

  list.PushBack(obj1.entry1);
  list.PushBack(obj2.entry1);
  list.PushBack(obj3.entry1);

  decltype(list)::Iter it = list.rbegin();
  EXPECT_EQ(it->value, 3);
  it++;
  EXPECT_EQ(it->value, 2);
  it++;
  EXPECT_EQ(it->value, 1);
  ++it;
  EXPECT_TRUE(!it);
  EXPECT_EQ(it, list.rend());
}

TEST(LinkedListTest, MultipleThreads) {
  LINKED_LIST(TestObject, entry1) list1;
  LINKED_LIST(TestObject, entry2) list2;

  TestObject obj1(1), obj2(2), obj3(3);

  list1.PushBack(obj1.entry1);
  list1.PushBack(obj2.entry1);
  list1.PushBack(obj3.entry1);

  list2.PushBack(obj2.entry2);
  list2.PushBack(obj3.entry2);
  list2.PushBack(obj1.entry2);

  decltype(list1)::Iter it1 = list1.begin();
  EXPECT_EQ(it1->value, 1);
  it1++;
  EXPECT_EQ(it1->value, 2);
  it1++;
  EXPECT_EQ(it1->value, 3);
  ++it1;
  EXPECT_EQ(it1, list1.end());

  decltype(list2)::Iter it2 = list2.begin();
  EXPECT_EQ(it2->value, 2);
  it2++;
  EXPECT_EQ(it2->value, 3);
  it2++;
  EXPECT_EQ(it2->value, 1);
  ++it2;
  EXPECT_EQ(it2, list2.end());
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
