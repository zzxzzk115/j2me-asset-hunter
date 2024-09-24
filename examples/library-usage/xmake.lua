-- target defination, name: library-usage
target("library-usage")
    -- set target kind: executable
    set_kind("binary")

    add_includedirs(".", { public = true })

    -- set values
    set_values("asset_files", "assets/**")

    -- add rules
    add_rules("copy_assets")

    -- add source files
    add_files("**.cpp")

    add_deps("j2me-asset-hunter-static-lib")

    -- set target directory
    set_targetdir("$(buildir)/$(plat)/$(arch)/$(mode)/library-usage")