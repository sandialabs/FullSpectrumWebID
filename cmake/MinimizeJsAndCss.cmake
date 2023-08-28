include(FindPackageHandleStandardArgs)

if(NOT UglifyJS_EXECUTABLE)
    find_program(UglifyJS_EXECUTABLE terser uglifyjs HINTS /usr/local/bin/ )
endif()

find_package_handle_standard_args(UglifyJS DEFAULT_MSG UglifyJS_EXECUTABLE)


if(NOT UglifyCSS_EXECUTABLE)
    find_program(UglifyCSS_EXECUTABLE uglifycss  HINTS /usr/local/bin/)
endif()

find_package_handle_standard_args(UglifyCSS DEFAULT_MSG UglifyCSS_EXECUTABLE)


macro( minimize_js_resource input output )
message( "Will deploy JS ${input} to ${output}")
  if( UglifyJS_EXECUTABLE )
    message( "Will uglify ${input} to ${output} using ${UglifyJS_EXECUTABLE}")
    add_custom_command(
        OUTPUT ${output}
        DEPENDS ${input}
        COMMENT "Minimizing ${input} to ${output}"
        COMMAND ${UglifyJS_EXECUTABLE} -c -o \"${output}\" \"${input}\" )
  else( UglifyJS_EXECUTABLE )
    message( "Will COPY ${input} to ${output}")
    add_custom_command( OUTPUT ${output} 
        COMMENT "Not minimizing, just copying ${input} to ${output}" 
        COMMAND ${CMAKE_COMMAND} -E copy ${input} ${output} 
        DEPENDS ${input} )
  endif( UglifyJS_EXECUTABLE )
endmacro( minimize_js_resource )

macro( minimize_css_resource input output )
   message( "Will deploy CSS ${input} to ${output}")
  if( UglifyCSS_EXECUTABLE )
    message( "Will uglify ${input} to ${output} using ${UglifyCSS_EXECUTABLE}")
    add_custom_command(
        OUTPUT ${output}
        DEPENDS ${input}
        COMMENT "Minimizing ${input} to ${output}"
        COMMAND ${UglifyCSS_EXECUTABLE} --output \"${output}\" \"${input}\"  )
  else( UglifyCSS_EXECUTABLE )
    message( "Will COPY ${input} to ${output}")
    add_custom_command( OUTPUT ${output} 
        COMMENT "Not minimizing, just copying ${input} to ${output}" 
        COMMAND ${CMAKE_COMMAND} -E copy ${input} ${output} 
        DEPENDS ${input} )
  endif( UglifyCSS_EXECUTABLE )
endmacro( minimize_css_resource )