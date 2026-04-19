add_executable(t6522 t6522.cpp)
add_executable(t6522b t6522b.cpp)
add_executable(n6522 n6522.cpp)

target_link_libraries(n6522 PRIVATE
    gs2_cpu    
    gs2_cpu_new
    gs2_trace
    gs2_util
)

target_link_libraries(t6522b PRIVATE
    gs2_cpu    
    gs2_cpu_new
    gs2_trace
)