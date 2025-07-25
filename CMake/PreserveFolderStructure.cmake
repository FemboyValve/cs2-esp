# Usage: preserve_folder_structure(<target> <list of source files>...)
function(preserve_folder_structure target)
    # ARGN holds everything after <target>
    foreach(src IN LISTS ARGN)
        # Make srcPath relative to the project root, e.g. "src/utils/log.cpp"
        file(RELATIVE_PATH srcPath "${CMAKE_SOURCE_DIR}" "${src}")
        # Get the directory portion, e.g. "src/utils"
        get_filename_component(srcDir "${srcPath}" PATH)
        # Convert forward‐slashes to Windows‐style backslashes (optional, for Visual Studio grouping)
        string(REPLACE "/" "\\" groupName "${srcDir}")
        # Create a SourceGroup matching the folder structure
        source_group("${groupName}" FILES "${src}")
    endforeach()
endfunction()