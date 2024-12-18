#include <gtest/gtest.h>


// top-level test entrypoint: just bootstraps the CLI and makes sure that the test
// runner actually functions. Application tests are placed in other compilation
// units

TEST(metasuite_1, test_1)
{
    // this is here to ensure that the test actually turns up in the output/IDE
}

TEST(metasuite_1, test_2)
{
    // this is here to ensure that the test actually turns up in the output/IDE
}

TEST(metasuite_2, test_1)
{
    // this is here to ensure that the test actually turns up in the output/IDE
    //
    // (should be grouped separately)
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
