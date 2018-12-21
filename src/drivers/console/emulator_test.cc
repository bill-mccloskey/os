#include "emulator.h"

#include <stdio.h>
#include <string>

#include "gtest/gtest.h"

struct Cell {
  wchar_t contents = ' ';
  CellAttributes attrs;
};

std::string g_test_dir;

class TestEmulatorOutput : public TerminalEmulatorOutput {
public:
  void SetCell(int x, int y, wchar_t ch, const CellAttributes& attrs) override {
    ASSERT_GE(x, 0);
    ASSERT_GE(y, 0);

    if (x < kWidth && y < kHeight) {
      cells[y][x].contents = ch;
      cells[y][x].attrs = attrs;
    }
  }

  wchar_t GetCell(int x, int y) {
    if (x >= 0 && x < kWidth && y >= 0 && y < kHeight) {
      return cells[y][x].contents;
    }
    return 0;
  }

  CellAttributes GetAttrs(int x, int y) {
    if (x >= 0 && x < kWidth && y >= 0 && y < kHeight) {
      return cells[y][x].attrs;
    }
    return CellAttributes();
  }

  void Bell() override {}

  void MoveCells(int dest_x, int dest_y, int src_x, int src_y, int width, int height) override;

  void CopyToScrollback(int src_y, int y_count) override;

  int Width() override { return kWidth; }
  int Height() override { return kHeight; }

  static const int kHeight = 5;
  static const int kWidth = 10;

  Cell cells[kHeight][kWidth];
};

void TestEmulatorOutput::MoveCells(int dest_x, int dest_y, int src_x, int src_y, int width, int height) {
  int start_x, end_x, x_incr;
  if (dest_x <= src_x) {
    start_x = 0;
    end_x = width;
    x_incr = 1;
  } else {
    start_x = width - 1;
    end_x = -1;
    x_incr = -1;
  }

  int start_y, end_y, y_incr;
  if (dest_y <= src_y) {
    start_y = 0;
    end_y = height;
    y_incr = 1;
  } else {
    start_y = height - 1;
    end_y = -1;
    y_incr = -1;
  }

  for (int y = start_y; y != end_y; y += y_incr) {
    for (int x = start_x; x != end_x; x += x_incr) {
      cells[dest_y + y][dest_x + x] = cells[src_y + y][src_x + x];
    }
  }
}

void TestEmulatorOutput::CopyToScrollback(int src_y, int y_count) {
}

class EmulatorTest : public testing::Test {
protected:
  EmulatorTest() : em(&out) {}

  void InputString(const char* s) {
    for (int i = 0; s[i]; i++) {
      em.Input(s[i]);
    }
  }

  TestEmulatorOutput out;
  TerminalEmulator em;
};

static std::string ReadTestData(const char* test_name) {
  char buffer[8192];
  std::string test_data = g_test_dir + "/" + test_name + ".data";
  FILE* fp = fopen(test_data.c_str(), "r");
  if (fp == nullptr) {
    return "";
  }
  size_t bytes = fread(buffer, 1, sizeof(buffer) - 1, fp);
  buffer[bytes] = 0;
  fclose(fp);

  return std::string(buffer);
}

TEST_F(EmulatorTest, Basic) {
  std::string data = ReadTestData("basic");

  for (char c : data) {
    em.Input(c);
  }

  for (int i = 0; i < TestEmulatorOutput::kHeight; i++) {
    for (int j = 0; j < TestEmulatorOutput::kWidth; j++) {
      printf("[%c] ", out.cells[i][j].contents);
    }
    printf("\n");
  }
}

TEST_F(EmulatorTest, BackspaceTest) {
  InputString("a\bb");

  EXPECT_EQ(out.GetCell(0, 0), 'b');
}

TEST_F(EmulatorTest, LineFeedTest) {
  InputString("a\nb");
  EXPECT_EQ(out.GetCell(0, 0), 'a');
  EXPECT_EQ(out.GetCell(1, 1), 'b');
}

TEST_F(EmulatorTest, CarriageReturnTest) {
  InputString("ab\rc");

  EXPECT_EQ(out.GetCell(0, 0), 'c');
  EXPECT_EQ(out.GetCell(1, 0), 'b');
}

TEST_F(EmulatorTest, TitleTest) {
  InputString("a\e]0;this is a test\ab");

  EXPECT_EQ(out.GetCell(0, 0), 'a');
  EXPECT_EQ(out.GetCell(1, 0), 'b');
}

TEST_F(EmulatorTest, BoldTest) {
  InputString("a\e[1mbc\e[2md");

  EXPECT_EQ(out.GetCell(0, 0), 'a');
  EXPECT_EQ(out.GetCell(1, 0), 'b');
  EXPECT_EQ(out.GetCell(2, 0), 'c');
  EXPECT_EQ(out.GetCell(3, 0), 'd');

  EXPECT_EQ(out.GetAttrs(0, 0), CellAttributes());
  EXPECT_EQ(out.GetAttrs(1, 0), CellAttributes().SetAttribute(CellAttributes::kBold));
  EXPECT_EQ(out.GetAttrs(2, 0), CellAttributes().SetAttribute(CellAttributes::kBold));
  EXPECT_EQ(out.GetAttrs(3, 0), CellAttributes().SetAttribute(CellAttributes::kFaint));
}

TEST_F(EmulatorTest, StylesTest) {
  InputString("a\e[3mb\e[4mc\e[5md\e[7me\e[27mf\e[mg");

  EXPECT_EQ(out.GetCell(0, 0), 'a');
  EXPECT_EQ(out.GetCell(1, 0), 'b');
  EXPECT_EQ(out.GetCell(2, 0), 'c');
  EXPECT_EQ(out.GetCell(3, 0), 'd');
  EXPECT_EQ(out.GetCell(4, 0), 'e');
  EXPECT_EQ(out.GetCell(5, 0), 'f');
  EXPECT_EQ(out.GetCell(6, 0), 'g');

  CellAttributes attrs;
  EXPECT_EQ(out.GetAttrs(0, 0), attrs);
  attrs.SetAttribute(CellAttributes::kItalic);
  EXPECT_EQ(out.GetAttrs(1, 0), attrs);
  attrs.SetAttribute(CellAttributes::kUnderline);
  EXPECT_EQ(out.GetAttrs(2, 0), attrs);
  attrs.SetAttribute(CellAttributes::kBlink);
  EXPECT_EQ(out.GetAttrs(3, 0), attrs);
  attrs.SetAttribute(CellAttributes::kInverse);
  EXPECT_EQ(out.GetAttrs(4, 0), attrs);
  attrs.ClearAttribute(CellAttributes::kInverse);
  EXPECT_EQ(out.GetAttrs(5, 0), attrs);
  EXPECT_EQ(out.GetAttrs(6, 0), CellAttributes());
}

TEST_F(EmulatorTest, MultiStylesTest) {
  InputString("a\e[3;4;5mb");

  EXPECT_EQ(out.GetCell(0, 0), 'a');
  EXPECT_EQ(out.GetCell(1, 0), 'b');

  CellAttributes attrs;
  EXPECT_EQ(out.GetAttrs(0, 0), attrs);
  attrs.SetAttribute(CellAttributes::kItalic);
  attrs.SetAttribute(CellAttributes::kUnderline);
  attrs.SetAttribute(CellAttributes::kBlink);
  EXPECT_EQ(out.GetAttrs(1, 0), attrs);
}

TEST_F(EmulatorTest, ColorsTest) {
  InputString("a\e[37mb\e[44mc\e[22md\e[0me");

  EXPECT_EQ(out.GetCell(0, 0), 'a');
  EXPECT_EQ(out.GetCell(1, 0), 'b');
  EXPECT_EQ(out.GetCell(2, 0), 'c');
  EXPECT_EQ(out.GetCell(3, 0), 'd');
  EXPECT_EQ(out.GetCell(4, 0), 'e');

  CellAttributes attrs;
  EXPECT_EQ(out.GetAttrs(0, 0), attrs);
  attrs.fg_color = CellAttributes::kWhite;
  EXPECT_EQ(out.GetAttrs(1, 0), attrs);
  attrs.bg_color = CellAttributes::kBlue;
  EXPECT_EQ(out.GetAttrs(2, 0), attrs);
  EXPECT_EQ(out.GetAttrs(3, 0), attrs);
  EXPECT_EQ(out.GetAttrs(4, 0), CellAttributes());
}

TEST_F(EmulatorTest, EraseCharsTest1) {
  InputString("abc\b\b\e[44m\e[P");

  EXPECT_EQ(out.GetCell(0, 0), 'a');
  EXPECT_EQ(out.GetCell(1, 0), 'c');
  EXPECT_EQ(out.GetCell(2, 0), ' ');

  CellAttributes attrs;
  EXPECT_EQ(out.GetAttrs(1, 0), attrs);
  EXPECT_EQ(out.GetAttrs(2, 0), attrs);
  EXPECT_EQ(out.GetAttrs(3, 0), attrs);
  EXPECT_EQ(out.GetAttrs(TestEmulatorOutput::kWidth - 2, 0), attrs);
  attrs.bg_color = CellAttributes::kBlue;
  EXPECT_EQ(out.GetAttrs(TestEmulatorOutput::kWidth - 1, 0), attrs);
}

TEST_F(EmulatorTest, EraseCharsTest2) {
  InputString("abc\b\b\e[44m\e[2P");

  EXPECT_EQ(out.GetCell(0, 0), 'a');
  EXPECT_EQ(out.GetCell(1, 0), ' ');
  EXPECT_EQ(out.GetCell(2, 0), ' ');

  CellAttributes attrs;
  EXPECT_EQ(out.GetAttrs(1, 0), attrs);
  EXPECT_EQ(out.GetAttrs(2, 0), attrs);
  EXPECT_EQ(out.GetAttrs(3, 0), attrs);
  EXPECT_EQ(out.GetAttrs(TestEmulatorOutput::kWidth - 3, 0), attrs);
  attrs.bg_color = CellAttributes::kBlue;
  EXPECT_EQ(out.GetAttrs(TestEmulatorOutput::kWidth - 2, 0), attrs);
  EXPECT_EQ(out.GetAttrs(TestEmulatorOutput::kWidth - 1, 0), attrs);
}

TEST_F(EmulatorTest, EraseInLineTest0) {
  InputString("abc\b\b\e[44m\e[K");

  EXPECT_EQ(out.GetCell(0, 0), 'a');
  EXPECT_EQ(out.GetCell(1, 0), ' ');
  EXPECT_EQ(out.GetCell(2, 0), ' ');

  CellAttributes attrs;
  EXPECT_EQ(out.GetAttrs(0, 0), attrs);
  attrs.bg_color = CellAttributes::kBlue;
  EXPECT_EQ(out.GetAttrs(1, 0), attrs);
  EXPECT_EQ(out.GetAttrs(2, 0), attrs);
  EXPECT_EQ(out.GetAttrs(TestEmulatorOutput::kWidth - 1, 0), attrs);
}

TEST_F(EmulatorTest, EraseInLineTest1) {
  InputString("abc\b\b\e[44m\e[1K");

  EXPECT_EQ(out.GetCell(0, 0), ' ');
  EXPECT_EQ(out.GetCell(1, 0), ' ');
  EXPECT_EQ(out.GetCell(2, 0), 'c');

  CellAttributes attrs;
  EXPECT_EQ(out.GetAttrs(2, 0), attrs);
  EXPECT_EQ(out.GetAttrs(TestEmulatorOutput::kWidth - 1, 0), attrs);
  attrs.bg_color = CellAttributes::kBlue;
  EXPECT_EQ(out.GetAttrs(0, 0), attrs);
  EXPECT_EQ(out.GetAttrs(1, 0), attrs);
}

TEST_F(EmulatorTest, EraseInLineTest2) {
  InputString("abc\b\b\e[44m\e[2K");

  EXPECT_EQ(out.GetCell(0, 0), ' ');
  EXPECT_EQ(out.GetCell(1, 0), ' ');
  EXPECT_EQ(out.GetCell(2, 0), ' ');

  CellAttributes attrs;
  attrs.bg_color = CellAttributes::kBlue;
  EXPECT_EQ(out.GetAttrs(0, 0), attrs);
  EXPECT_EQ(out.GetAttrs(1, 0), attrs);
  EXPECT_EQ(out.GetAttrs(2, 0), attrs);
  EXPECT_EQ(out.GetAttrs(TestEmulatorOutput::kWidth - 1, 0), attrs);
}

int main(int argc, char** argv) {
  std::string path = argv[0];
  size_t pos = path.rfind('/');
  if (pos == std::string::npos) {
    g_test_dir = "emulator_test_data";
  } else {
    g_test_dir = path.substr(0, pos + 1) + "emulator_test_data";
  }

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
