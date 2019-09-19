#include <attributes.h>

#include <gtest/gtest.h>

#include <string>

TEST(AttributeContainer, Empty)
{
    AttributeContainer container;

    EXPECT_FALSE(container.hasAttr(0));
    EXPECT_FALSE(container.hasAttrString(0));
    EXPECT_FALSE(container.hasAttrInteger(0));

    EXPECT_EQ(0, container.getAttrAs<int>(0));
    EXPECT_EQ(std::string{}, container.getAttrAs<std::string>(0));
}

template <typename T>
struct Param
{
    using Type = T;
    using OtherType = std::string;

    const Type VALUE = std::numeric_limits<T>::max();
    const Type OTHER_VALUE = std::numeric_limits<T>::min();
};

template <>
struct Param<std::string>
{
    using Type = std::string;
    using OtherType = int;

    const Type VALUE = "STRING VALUE";
    const Type OTHER_VALUE = "ANOTHER STRING VALUE";
};

template <typename T>
class AttributeContainerT : public ::testing::Test
{
};

//using MyTypes = ::testing::Types<int, std::string>;
typedef ::testing::Types<int, std::string> MyTypes;
TYPED_TEST_CASE(AttributeContainerT, MyTypes);

TYPED_TEST(AttributeContainerT, SetAttribute)
{
    // This should go to fixture, but that makes it akward to use (requires 'this->' on KEY, VALUE, etc.).
    using Type = typename Param<TypeParam>::Type;
    using OtherType = typename Param<TypeParam>::OtherType;
    const int KEY = 1;
    const TypeParam VALUE = Param<TypeParam>{}.VALUE;
    const TypeParam OTHER_VALUE = Param<TypeParam>{}.OTHER_VALUE;

    AttributeContainer container;

    EXPECT_NO_THROW(container.setAttr(KEY, VALUE));

    EXPECT_TRUE(container.hasAttr(KEY));
    EXPECT_TRUE(container.hasAttrAs<Type>(KEY));
    EXPECT_FALSE(container.hasAttrAs<OtherType>(KEY));

    EXPECT_EQ(VALUE, container.getAttrAs<Type>(KEY));
    EXPECT_EQ(OtherType{}, container.getAttrAs<OtherType>(KEY));
}

TYPED_TEST(AttributeContainerT, ResetAttribute)
{
    using Type = typename Param<TypeParam>::Type;
    using OtherType = typename Param<TypeParam>::OtherType;
    const int KEY = 1;
    const TypeParam VALUE = Param<TypeParam>{}.VALUE;
    const TypeParam OTHER_VALUE = Param<TypeParam>{}.OTHER_VALUE;

    AttributeContainer container;

    EXPECT_NO_THROW(container.setAttr(KEY, VALUE));
    EXPECT_NO_THROW(container.setAttr(KEY, OTHER_VALUE));

    EXPECT_TRUE(container.hasAttr(KEY));
    EXPECT_TRUE(container.hasAttrAs<Type>(KEY));
    EXPECT_FALSE(container.hasAttrAs<OtherType>(KEY));

    EXPECT_EQ(OTHER_VALUE, container.getAttrAs<Type>(KEY));
    EXPECT_EQ(OtherType{}, container.getAttrAs<OtherType>(KEY));
}

class AttributeContainerTrackingAttributeChange : public AttributeContainer
{
public:
    std::vector<int> changed_attributes;

protected:
    void onAttrChange(int attr) override
    {
        changed_attributes.push_back(attr);
    }
};

TYPED_TEST(AttributeContainerT, SetAttributeCallBack)
{
    const int KEY = 1;
    const TypeParam VALUE = Param<TypeParam>{}.VALUE;
    const TypeParam OTHER_VALUE = Param<TypeParam>{}.OTHER_VALUE;

    AttributeContainerTrackingAttributeChange container;

    EXPECT_NO_THROW(container.setAttr(KEY, VALUE));
    ASSERT_EQ(1, container.changed_attributes.size());
    EXPECT_EQ(KEY, container.changed_attributes.back());

    EXPECT_NO_THROW(container.setAttr(KEY, VALUE));
    ASSERT_EQ(2, container.changed_attributes.size());
    EXPECT_EQ(KEY, container.changed_attributes.back());

    const auto NEW_KEY = KEY + 1;
    EXPECT_NO_THROW(container.setAttr(NEW_KEY, VALUE));
    ASSERT_EQ(3, container.changed_attributes.size());
    EXPECT_EQ(NEW_KEY, container.changed_attributes.back());
}

TYPED_TEST(AttributeContainerT, SetAttributeSilentCallBack)
{
    const int KEY = 1;
    const TypeParam VALUE = Param<TypeParam>{}.VALUE;
    const TypeParam OTHER_VALUE = Param<TypeParam>{}.OTHER_VALUE;

    AttributeContainerTrackingAttributeChange container;

    EXPECT_NO_THROW(container.setAttrSilent(KEY, VALUE));
    ASSERT_EQ(0, container.changed_attributes.size());

    EXPECT_NO_THROW(container.setAttrSilent(KEY, OTHER_VALUE));
    ASSERT_EQ(0, container.changed_attributes.size());
}
