add_library(Wfdb wfdb.cc)

target_link_libraries(Wfdb absl::strings absl::status)
