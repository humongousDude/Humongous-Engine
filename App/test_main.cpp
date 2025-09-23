#include <all_tests.hpp>
#include <cstdlib>
#include <logger.hpp>

int main(int argc, char* argv[])
{
    // PauseLogging();
    ::testing::InitGoogleTest(&argc, argv);
    auto ret = RUN_ALL_TESTS();
    // ResumeLogging();

    if(ret != 0)
    {
        HGFATAL("Tests failed");
        return EXIT_FAILURE;
    }

    HGINFO("Tests passed");
    return EXIT_SUCCESS;
}
