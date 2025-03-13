#include "sql_rc.h"

#include <ostream>

#include "gtest/gtest.h"

// Test the integer value of the enum
TEST(u32ValueTest, HandlesOk) {
  EXPECT_EQ(0, static_cast<int>(ResultCode::kOk));
}

TEST(u32ValueTest, HandlesError) {
  EXPECT_EQ(1, static_cast<int>(ResultCode::kError));
}

TEST(u32ValueTest, HandlesCantOpen) {
  EXPECT_EQ(14, static_cast<int>(ResultCode::kCantOpen));
}

TEST(u32ValueTest, IOErrorRead) {
  EXPECT_EQ(266, static_cast<u32>(ResultCode::kIOErrorRead));
}

// Test the ResultCodeToString() function
TEST(ToStringTest, HandlesOk) {
  ResultCode code = ResultCode::kOk;
  std::string str_code = ToString(code);
  EXPECT_EQ("OK", str_code);
}

TEST(ToStringTest, HandlesError) {
  ResultCode code = ResultCode::kError;
  std::string str_code = ToString(code);
  EXPECT_EQ("ERROR", str_code);
}

TEST(ToStringTest, HandlesCantOpen) {
  ResultCode code = ResultCode::kCantOpen;
  std::string str_code = ToString(code);
  EXPECT_EQ("CANT_OPEN", str_code);
}

TEST(ToStringTest, HandlesIOErrorRead) {
  ResultCode code = ResultCode::kIOErrorRead;
  std::string str_code = ToString(code);
  EXPECT_EQ("IO_ERROR_READ", str_code);
}

// Test the << operator
TEST(OstreamTest, HandlesOk) {
  std::ostringstream os;
  os << ResultCode::kOk;
  EXPECT_EQ("OK", os.str());
}

TEST(OstreamTest, HandlesError) {
  std::ostringstream os;
  os << ResultCode::kError;
  EXPECT_EQ("ERROR", os.str());
}

TEST(OstreamTest, HandlesCantOpen) {
  std::ostringstream os;
  os << ResultCode::kCantOpen;
  EXPECT_EQ("CANT_OPEN", os.str());
}

TEST(OstreamTest, IOErrorRead) {
  std::ostringstream os;
  os << ResultCode::kIOErrorRead;
  EXPECT_EQ("IO_ERROR_READ", os.str());
}

TEST(OstreamTest, HandlesOkWithOtherText) {
  std::ostringstream os;
  os << "The return code is " << ResultCode::kOk << " and that's it.";
  EXPECT_EQ("The return code is OK and that's it.", os.str());
}


// Test the GetPrimaryResultCode() function
TEST(GetPrimaryTest, AuthError) {
  // ResultCode::kAuth is a primary result code
  // When passed into the function, I expect the same result code to be returned
  ResultCode rc = ResultCode::kAuth;
  EXPECT_EQ(rc, GetPrimaryResultCode(rc));
}

TEST(GetPrimaryTest, AuthUserError) {
  // ResultCode::kAuthUser is an extended result code of ResultCode::kAuth
  // When passed into the function, I expect its primary result code to be returned
  ResultCode extended_rc = ResultCode::kAuthUser;
  ResultCode primary_rc = ResultCode::kAuth;
  EXPECT_EQ(primary_rc, GetPrimaryResultCode(extended_rc));
}
