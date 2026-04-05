cmake_minimum_required(VERSION 3.24)

project(SoloPlayNativeHostPatch NONE)

set(PATCH_NAME "SoloPlayNativeHostPatch")
set(PATCH_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/dllmain.cpp")
set(PATCH_OBJ "${PATCH_NAME}.obj")
set(PATCH_DLL "${PATCH_NAME}.dll")
set(XINPUT_NAME "XINPUT9_1_0")
set(XINPUT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/xinput_proxy.cpp")
set(XINPUT_OBJ "${XINPUT_NAME}.obj")
set(XINPUT_DLL "${XINPUT_NAME}.dll")
set(DXVA2_NAME "dxva2")
set(DXVA2_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/dxva2_proxy.cpp")
set(DXVA2_OBJ "${DXVA2_NAME}.obj")
set(DXVA2_DLL "${DXVA2_NAME}.dll")
set(GAME_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../binaries")

add_custom_command(
    OUTPUT "${PATCH_OBJ}"
    COMMAND clang++ --target=x86_64-pc-windows-msvc -DUNICODE -D_UNICODE -fno-exceptions -fno-rtti -fno-builtin -fms-extensions -O2 -c "${PATCH_SRC}" -o "${PATCH_OBJ}"
    DEPENDS "${PATCH_SRC}"
    COMMENT "Compiling ${PATCH_NAME}.obj"
    VERBATIM
)

add_custom_command(
    OUTPUT "${PATCH_DLL}"
    COMMAND lld-link /dll /out:${PATCH_DLL} /entry:DllMain /nodefaultlib /machine:x64 ${PATCH_OBJ}
    DEPENDS "${PATCH_OBJ}"
    COMMENT "Linking ${PATCH_NAME}.dll"
    VERBATIM
)

add_custom_target(${PATCH_NAME} ALL DEPENDS "${PATCH_DLL}")

add_custom_command(
    OUTPUT "${XINPUT_OBJ}"
    COMMAND clang++ --target=x86_64-pc-windows-msvc -DUNICODE -D_UNICODE -fno-exceptions -fno-rtti -fno-builtin -fms-extensions -O2 -c "${XINPUT_SRC}" -o "${XINPUT_OBJ}"
    DEPENDS "${XINPUT_SRC}"
    COMMENT "Compiling ${XINPUT_NAME}.obj"
    VERBATIM
)

add_custom_command(
    OUTPUT "${XINPUT_DLL}"
    COMMAND lld-link /dll /out:${XINPUT_DLL} /entry:DllMain /nodefaultlib /machine:x64 ${XINPUT_OBJ}
    DEPENDS "${XINPUT_OBJ}"
    COMMENT "Linking ${XINPUT_NAME}.dll"
    VERBATIM
)

add_custom_target(${XINPUT_NAME} ALL DEPENDS "${XINPUT_DLL}")

add_custom_command(
    OUTPUT "${DXVA2_OBJ}"
    COMMAND clang++ --target=x86_64-pc-windows-msvc -DUNICODE -D_UNICODE -fno-exceptions -fno-rtti -fno-builtin -fms-extensions -O2 -c "${DXVA2_SRC}" -o "${DXVA2_OBJ}"
    DEPENDS "${DXVA2_SRC}"
    COMMENT "Compiling ${DXVA2_NAME}.obj"
    VERBATIM
)

add_custom_command(
    OUTPUT "${DXVA2_DLL}"
    COMMAND lld-link /dll /out:${DXVA2_DLL} /entry:DllMain /nodefaultlib /machine:x64 ${DXVA2_OBJ}
    DEPENDS "${DXVA2_OBJ}"
    COMMENT "Linking ${DXVA2_NAME}.dll"
    VERBATIM
)

add_custom_target(${DXVA2_NAME} ALL DEPENDS "${DXVA2_DLL}")

add_custom_target(deploy_proxy
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/${PATCH_DLL}" "${GAME_BIN_DIR}/${PATCH_NAME}.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/${XINPUT_DLL}" "${GAME_BIN_DIR}/${XINPUT_NAME}.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/${DXVA2_DLL}" "${GAME_BIN_DIR}/${DXVA2_DLL}"
    DEPENDS ${PATCH_NAME} ${XINPUT_NAME} ${DXVA2_NAME}
    COMMENT "Copying native patch DLLs into Darktide binaries"
    VERBATIM
)
