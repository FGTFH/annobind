# 用于就地添加nanobind模块的辅助函数

function(nanobind_custom_build MODULE_NAME DESTINATION)
    # pyi生成与文件拷贝
    add_custom_command(
        TARGET ${MODULE_NAME} POST_BUILD
        COMMAND ${Python_EXECUTABLE} -m nanobind.stubgen -m ${MODULE_NAME} -O ./stub --recursive

        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating ${MODULE_NAME}.pyi file"
    )

    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/stub/
        DESTINATION ${DESTINATION}
        FILES_MATCHING PATTERN "*.pyi")
    install(FILES $<TARGET_FILE:${MODULE_NAME}> DESTINATION ${DESTINATION})

    # 添加nanobind包含目录
    target_include_directories(${MODULE_NAME}
        PRIVATE
        ${Python_INCLUDE_DIRS}
        ${Python_ROOT_DIR}/Lib/site-packages/nanobind/include
    )

    # 到源文件的宏定义导出
    target_compile_definitions(${MODULE_NAME} PRIVATE
        PROJECT_NAME=${MODULE_NAME} # 模块名称, 用在 nanobind_MODULE 处
        SRC_BASEDIR="${CMAKE_CURRENT_SOURCE_DIR}" # 工作目录, 用于 DETAILED_DOC 生成
    )
endfunction(nanobind_custom_build)
