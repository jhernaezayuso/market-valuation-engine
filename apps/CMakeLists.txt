add_executable(swap_cva swap_cva/main.cpp)

target_link_libraries(swap_cva
    PRIVATE
    xva_core
)
