# header-only library
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO alandtse/MergeMapper
    REF 9d710f2a2e055acb13e41319cc19e3255fa0538a
    SHA512 4e9b49aaaf5ac969956bf066cb92423b42a4ba6681e188b792ee3494fba3084dccc744dfc4722ee8d8df1b245e5ee4b20a503e9610fadb546df921e6fae42506
    HEAD_REF main
)

# Install codes
set(MERGEMAPPER_SOURCE ${SOURCE_PATH}/src/MergeMapperPluginAPI.cpp ${SOURCE_PATH}/include/MergeMapperPluginAPI.h)

file(INSTALL ${MERGEMAPPER_SOURCE} DESTINATION ${CURRENT_PACKAGES_DIR}/include)

file(INSTALL ${SOURCE_PATH}/LICENSE DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
