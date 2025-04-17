#include "driver/platform/platform.h"
#include "driver/test/client_utils.h"
#include "driver/test/client_test_base.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

enum class PlaceToCheck {
    AfterPrepare,
    AfterExecute,
    AfterFirstFetch,
    AfterLastFetch
};

enum class CallToCheck {
    DescribeCol,
    ColumnAttributes
};

enum class TypeToCheck {
    Int8,
    UInt8
};

std::ostream & operator<< (std::ostream & os, const PlaceToCheck & type_to_check) {
    switch (type_to_check) {
        case PlaceToCheck::AfterPrepare:    os << "AfterPrepare";    break;
        case PlaceToCheck::AfterExecute:    os << "AfterExecute";    break;
        case PlaceToCheck::AfterFirstFetch: os << "AfterFirstFetch"; break;
        case PlaceToCheck::AfterLastFetch:  os << "AfterLastFetch";  break;
        default: throw std::runtime_error("unknown place");
    }
    return os;
}

std::ostream & operator<< (std::ostream & os, const CallToCheck & call_to_check) {
    switch (call_to_check) {
        case CallToCheck::DescribeCol:      os << "DescribeCol";      break;
        case CallToCheck::ColumnAttributes: os << "ColumnAttributes"; break;
        default: throw std::runtime_error("unknown call");
    }
    return os;
}

std::ostream & operator<< (std::ostream & os, const TypeToCheck & type_to_check) {
    switch (type_to_check) {
        case TypeToCheck::Int8:  os << "Int8";  break;
        case TypeToCheck::UInt8: os << "UInt8"; break;
        default: throw std::runtime_error("unknown call");
    }
    return os;
}

class CheckerBase {
public:
    explicit CheckerBase(SQLHSTMT hstmt)
        : hstmt_(hstmt)
    {
    }

    virtual ~CheckerBase() = default;

    virtual std::string getTypeNameAsString() const = 0;
    virtual std::string getDefaultValueAsString() const = 0;
    virtual void checkDescribeCol() = 0;
    virtual void checkColumnAttributes() = 0;
    virtual void bindTypedBuffer() = 0;
    virtual void checkData() = 0;

protected:
    const SQLHSTMT hstmt_;
};

std::unique_ptr<CheckerBase> makeChecker(SQLHSTMT hstmt, TypeToCheck type_to_check);

class ColumnTypeInfoTest
    : public ClientTestBase
    , public ::testing::WithParamInterface<std::tuple<PlaceToCheck, CallToCheck, TypeToCheck>>
{
private:
    static std::string composeQuery(CheckerBase & checker) {
        return "SELECT CAST('" + checker.getDefaultValueAsString() + "', '" + checker.getTypeNameAsString() + "') AS col FROM numbers(2)";
    }

    static void checkColumnInfo(CheckerBase & checker, CallToCheck call_to_check) {
        switch (call_to_check) {
            case CallToCheck::DescribeCol:      checker.checkDescribeCol(); break;
            case CallToCheck::ColumnAttributes: checker.checkColumnAttributes(); break;
            default:                            throw std::runtime_error("unknown call");
        }
    }

    static void bindTypedBuffer(CheckerBase & checker) {
        checker.bindTypedBuffer();
    }

    static void checkData(CheckerBase & checker) {
        checker.checkData();
    }

public:
    void execute(PlaceToCheck place_to_check, CallToCheck call_to_check, TypeToCheck type_to_check) {
        std::unique_ptr<CheckerBase> checker = makeChecker(hstmt, type_to_check);

        const auto query = fromUTF8<SQLTCHAR>(composeQuery(*checker));
        auto * query_wptr = const_cast<SQLTCHAR * >(query.c_str());

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, query_wptr, SQL_NTS));
        if (place_to_check == PlaceToCheck::AfterPrepare)
            checkColumnInfo(*checker, call_to_check);

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));
        if (place_to_check == PlaceToCheck::AfterExecute)
            checkColumnInfo(*checker, call_to_check);

        bindTypedBuffer(*checker);

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLFetch(hstmt));
        if (place_to_check == PlaceToCheck::AfterFirstFetch)
            checkColumnInfo(*checker, call_to_check);

        checkData(*checker);

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLFetch(hstmt));
        if (place_to_check == PlaceToCheck::AfterLastFetch)
            checkColumnInfo(*checker, call_to_check);

        checkData(*checker);

        ASSERT_EQ(SQLFetch(hstmt), SQL_NO_DATA);
    }
};

template <TypeToCheck> class Checker; // Leave unimplemented for general case.

struct DescribeColResults {
    SQLSMALLINT data_type = std::numeric_limits<SQLSMALLINT>::max();      // not SQL_UNKNOWN_TYPE
    SQLULEN column_size = std::numeric_limits<SQLULEN>::max();            // not SQL_NO_TOTAL
    SQLSMALLINT decimal_digits = std::numeric_limits<SQLSMALLINT>::max(); // not 0
    SQLSMALLINT nullable = std::numeric_limits<SQLSMALLINT>::max();       // not SQL_NULLABLE_UNKNOWN
};

struct ColumnAttributesResults {




};

template <typename CheckerType, typename PODType>
class PODCheckerBase
    : public CheckerBase
{
private: // CRTP static upcasts.
    CheckerType & self() noexcept {
        return *static_cast<CheckerType *>(this);
    }

    const CheckerType & self() const noexcept {
        return *static_cast<const CheckerType *>(this);
    }

public:
    using NativeType = PODType;

    using CheckerBase::CheckerBase;
    virtual ~PODCheckerBase() = default;

    virtual std::string getDefaultValueAsString() const override {
        std::string value_str;
        value_manip::from_value<NativeType>::template to_value<std::string>::convert(self().getDefaultValue(), value_str);
        return value_str;
    }

    virtual void checkDescribeCol() override {
        DescribeColResults res;

        SQLTCHAR name[256] = {};
        SQLSMALLINT name_length = 0;

        ODBC_CALL_ON_STMT_THROW(hstmt_,
            SQLDescribeCol(
                hstmt_,
                1,
                name,
                lengthof(name),
                &name_length,
                &res.data_type,
                &res.column_size,
                &res.decimal_digits,
                &res.nullable
            )
        );

        const auto name_str = toUTF8(name);

        EXPECT_EQ(name_str, "col");
        EXPECT_EQ(name_length, 3);

        SQLSMALLINT name_length_only = 0;

        ODBC_CALL_ON_STMT_THROW(hstmt_,
            SQLDescribeCol(
                hstmt_,
                1,
                nullptr,
                0,
                &name_length_only,
                nullptr,
                nullptr,
                nullptr,
                nullptr
            )
        );

        EXPECT_EQ(name_length_only, name_length);

        self().checkDescribeColResults(res);
    }

    virtual void checkColumnAttributes() override {
        ColumnAttributesResults res;


        self().checkColumnAttributesResults(res);
    }

    virtual void bindTypedBuffer() override {
        value_manip::to_null(bound_value_);
        bound_value_ind_ = 0;

        ODBC_CALL_ON_STMT_THROW(hstmt_,
            SQLBindCol(
                hstmt_,
                1,
                getCTypeFor<decltype(bound_value_)>(),
                &bound_value_,
                sizeof(bound_value_),
                &bound_value_ind_
            )
        );

        bound_ = true;
    }

    virtual void checkData() override {
        if (bound_) {
            EXPECT_TRUE(bound_value_ind_ >= 0 || bound_value_ind_ == SQL_NTS);
            EXPECT_EQ(bound_value_, self().getDefaultValue());
        }

        NativeType value;
        value_manip::to_null(value);

        SQLLEN col_ind = 0;

        ODBC_CALL_ON_STMT_THROW(hstmt_,
            SQLGetData(
                hstmt_,
                1,
                getCTypeFor<decltype(value)>(),
                &value,
                sizeof(value),
                &col_ind
            )
        );

        EXPECT_TRUE(col_ind >= 0 || col_ind == SQL_NTS);
        EXPECT_EQ(value, self().getDefaultValue());
    }

private:
    bool bound_ = false;
    NativeType bound_value_;
    SQLLEN bound_value_ind_;
};

template <>
class Checker<TypeToCheck::Int8>
    : public PODCheckerBase<Checker<TypeToCheck::Int8>, SQLSCHAR>
{
public:
    using BaseType = PODCheckerBase<Checker<TypeToCheck::Int8>, SQLSCHAR>;

    using BaseType::BaseType;
    virtual ~Checker() = default;

    NativeType getDefaultValue() const {
        return -123;
    }

    virtual std::string getTypeNameAsString() const override {
        return "Int8";
    }

    void checkDescribeColResults(const DescribeColResults & res) {
        EXPECT_EQ(res.data_type, SQL_TINYINT);
        EXPECT_EQ(res.column_size, 4);
        EXPECT_EQ(res.decimal_digits, 0);
        EXPECT_EQ(res.nullable, SQL_NO_NULLS);
    }

    void checkColumnAttributesResults(const ColumnAttributesResults & res) {
//        EXPECT_EQ(res.data_type, SQL_TINYINT);





    }
};

template <>
class Checker<TypeToCheck::UInt8>
    : public PODCheckerBase<Checker<TypeToCheck::UInt8>, SQLCHAR>
{
public:
    using BaseType = PODCheckerBase<Checker<TypeToCheck::UInt8>, SQLCHAR>;

    using BaseType::BaseType;
    virtual ~Checker() = default;

    NativeType getDefaultValue() const {
        return 234;
    }

    virtual std::string getTypeNameAsString() const override {
        return "UInt8";
    }

    void checkDescribeColResults(const DescribeColResults & res) {
        EXPECT_EQ(res.data_type, SQL_TINYINT);
        EXPECT_EQ(res.column_size, 3);
        EXPECT_EQ(res.decimal_digits, 0);
        EXPECT_EQ(res.nullable, SQL_NO_NULLS);
    }

    void checkColumnAttributesResults(const ColumnAttributesResults & res) {






    }
};

std::unique_ptr<CheckerBase> makeChecker(SQLHSTMT hstmt, TypeToCheck type_to_check) {
    switch (type_to_check) {
        case TypeToCheck::Int8:  return std::make_unique<Checker< TypeToCheck::Int8  >>(hstmt);
        case TypeToCheck::UInt8: return std::make_unique<Checker< TypeToCheck::UInt8 >>(hstmt);

        default: throw std::runtime_error("unknown type");
    }
}

TEST_P(ColumnTypeInfoTest, Execute) {
    execute(std::get<0>(GetParam()), std::get<1>(GetParam()), std::get<2>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(SyntheticColumn, ColumnTypeInfoTest, ::testing::Combine(
    ::testing::Values(
//      PlaceToCheck::AfterPrepare, // Not supported - Column info is not available
        PlaceToCheck::AfterExecute,
        PlaceToCheck::AfterFirstFetch,
        PlaceToCheck::AfterLastFetch
    ),
    ::testing::Values(
        CallToCheck::DescribeCol,
        CallToCheck::ColumnAttributes
    ),
    ::testing::Values(
        TypeToCheck::Int8,
        TypeToCheck::UInt8
    )
));
