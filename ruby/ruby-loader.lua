-- mod-version:3

local ruby = require "libraries.ruby"
local core = require "core"
local config = require "core.config"

table.insert(package.searchers, function(modname)
  local ruby_paths = package.path:gsub("%.lua", ".rb")
  local path, err = package.searchpath(modname, ruby_paths)
  if not path then return err end
  return ruby.exec, path
end)

local mod_version_regex =
  regex.compile([[#.*mod-version:(\d+)(?:\.(\d+))?(?:\.(\d+))?(?:$|\s)]])
local function get_plugin_details(filename)
  local info = system.get_file_info(filename)
  if info ~= nil and info.type == "dir" then
    filename = filename .. PATHSEP .. "init.rb"
    info = system.get_file_info(filename)
  end
  if not info or not filename:match("%.rb$") then return false end
  local f = io.open(filename, "r")
  if not f then return false end
  local priority = false
  local version_match = false
  local major, minor, patch

  for line in f:lines() do
    if not version_match then
      local _major, _minor, _patch = mod_version_regex:match(line)
      
      if _major then
        _major = tonumber(_major) or 0
        _minor = tonumber(_minor) or 0
        _patch = tonumber(_patch) or 0
        major, minor, patch = _major, _minor, _patch

        version_match = major == MOD_VERSION_MAJOR
        if version_match then
          version_match = minor <= MOD_VERSION_MINOR
        end
        if version_match then
          version_match = patch <= MOD_VERSION_PATCH
        end
      end
    end

    if not priority then
      priority = line:match('%#.*%f[%a]priority%s*:%s*(%d+)')
      if priority then priority = tonumber(priority) end
    end

    if version_match then
      break
    end
  end
  f:close()
  return true, {
    version_match = version_match,
    version = major and {major, minor, patch} or {},
    priority = priority or 100
  }
end


local function load_ruby_plugins()
  local no_errors = true
  local refused_list = {
    userdir = {dir = USERDIR, plugins = {}},
    datadir = {dir = DATADIR, plugins = {}},
  }
  local files, ordered = {}, {}
  for _, root_dir in ipairs {DATADIR, USERDIR} do
    local plugin_dir = root_dir .. PATHSEP .. "plugins"
    for _, filename in ipairs(system.list_dir(plugin_dir) or {}) do
      if not files[filename] then
        table.insert(
          ordered, {file = filename}
        )
      end
      -- user plugins will always replace system plugins
      files[filename] = plugin_dir
    end
  end

  for _, plugin in ipairs(ordered) do
    local dir = files[plugin.file]
    local name = plugin.file:match("(.-)%.rb$") or plugin.file
    local is_ruby_file, details = get_plugin_details(dir .. PATHSEP .. plugin.file)

    plugin.valid = is_ruby_file
    plugin.name = name
    plugin.dir = dir
    plugin.priority = details and details.priority or 100
    plugin.version_match = details and details.version_match or false
    plugin.version = details and details.version or {}
    plugin.version_string = #plugin.version > 0 and table.concat(plugin.version, ".") or "unknown"
  end

  -- sort by priority or name for plugins that have same priority
  table.sort(ordered, function(a, b)
    if a.priority ~= b.priority then
      return a.priority < b.priority
    end
    return a.name < b.name
  end)

  local load_start = system.get_time()
  for _, plugin in ipairs(ordered) do
    if plugin.valid then
      if not config.skip_plugins_version and not plugin.version_match then
        core.log_quiet(
          "Version mismatch for plugin %q[%s] from %s",
          plugin.name,
          plugin.version_string,
          plugin.dir
        )
        local rlist = plugin.dir:find(USERDIR, 1, true) == 1
          and 'userdir' or 'datadir'
        local list = refused_list[rlist].plugins
        table.insert(list, plugin)
      elseif config.plugins[plugin.name] ~= false then
        local start = system.get_time()
        local ok, loaded_plugin = core.try(require, "plugins." .. plugin.name)
        if ok then
          local plugin_version = ""
          if plugin.version_string ~= MOD_VERSION_STRING then
            plugin_version = "["..plugin.version_string.."]"
          end
          core.log_quiet(
            "Loaded plugin %q%s from %s in %.1fms",
            plugin.name,
            plugin_version,
            plugin.dir,
            (system.get_time() - start) * 1000
          )
        end
        if not ok then
          no_errors = false
        elseif config.plugins[plugin.name].onload then
          core.try(config.plugins[plugin.name].onload, loaded_plugin)
        end
      end
    end
  end
  core.log_quiet(
    "Loaded all plugins in %.1fms",
    (system.get_time() - load_start) * 1000
  )
  return no_errors, refused_list
end

load_ruby_plugins();

return ruby
