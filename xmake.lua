-- set project name
set_project("j2me-asset-hunter")

-- set project version
set_version("1.0.0")

-- set language version: C++ 20
set_languages("cxx20")

-- global options
option("examples") -- build examples?
    set_default(true)
option_end()

-- if build on windows
if is_plat("windows") then
    add_cxxflags("/EHsc")
    add_cxxflags("/bigobj")
    if is_mode("debug") then
        set_runtimes("MDd")
        add_links("ucrtd")
    else
        set_runtimes("MD")
    end
else
    add_cxxflags("-fexceptions")
end

-- global rules
rule("copy_assets")
    after_build(function (target)
        local asset_files = target:values("asset_files")
        if asset_files then
            for _, file in ipairs(asset_files) do
                local relpath = path.relative(file, os.projectdir())
                local target_dir = path.join(target:targetdir(), path.directory(relpath))
                os.mkdir(target_dir)
                os.cp(file, target_dir)
                print("Copying asset: " .. file .. " -> " .. target_dir)
            end
        end
    end)
rule_end()

add_rules("mode.debug", "mode.release")
add_rules("plugin.vsxmake.autoupdate")
add_rules("plugin.compile_commands.autoupdate", {outputdir = ".vscode"})

-- add my own xmake-repo here
add_repositories("my-xmake-repo https://github.com/zzxzzk115/xmake-repo.git dev")

-- add requirements
add_requires("argparse", "libzip", "fluidsynth")

-- target defination, name: j2me-asset-hunter-static-lib
target("j2me-asset-hunter-static-lib")
    -- set target kind: static lib
    set_kind("static")

    add_includedirs("include", { public = true })

    -- add/remove header & source files
    add_headerfiles("include/(j2me-asset-hunter/**.hpp)")
    add_files("src/**.cpp")
    remove_files("src/main.cpp")

    -- add packages
    add_packages("argparse", { public = true })
    add_packages("libzip", { public = true })
    add_packages("fluidsynth", { public = true })

-- target defination, name: j2me-asset-hunter
target("j2me-asset-hunter")
    -- set target kind: executable
    set_kind("binary")

    -- set values
    set_values("asset_files", "assets/**")

    -- add rules
    add_rules("copy_assets")

    add_includedirs("include", { public = true })

    -- add header & source files
    add_files("src/main.cpp")

    add_deps("j2me-asset-hunter-static-lib")

-- if build examples, then include examples
if has_config("examples") then
    includes("examples")
end

-- pack
includes("@builtin/xpack")

xpack("j2me-asset-hunter")
    set_title("J2ME Asset Hunter ($(arch)) by Lazy_V")
    set_author("zzxzzk115@gmail.com")
    set_maintainer("zzxzzk115@gmail.com")
    set_copyright("Copyright (C) 2017-present, Lazy_V")
    set_license("MIT")
    set_licensefile("LICENSE")
    set_homepage("https://github.com/zzxzzk115/j2me-asset-hunter")

    set_formats("nsis", "zip", "srczip")
    add_installfiles("(assets/**)", {prefixdir = "bin"})
    add_sourcefiles("(include/**.hpp)")
    add_sourcefiles("(src/**.cpp)")
    add_targets("j2me-asset-hunter")
    on_load(function (package)
        if package:from_source() then
            package:set("basename", "j2me-asset-hunter-$(plat)-src-v$(version)")
        else
            package:set("basename", "j2me-asset-hunter-$(plat)-$(arch)-v$(version)")
        end
    end)
