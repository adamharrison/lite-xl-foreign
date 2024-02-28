# lite-xl-foreign

A simple repository that bundles intepreters, and allows you to write plugins in langauges that aren't lua for `lite-xl`.

## Description

You may ask: dear god, *why*?

Better question: **why not**?

## Examples

Installing the `ruby-loader` plugin allows you to do things like installing the following plugin into your normal folder plugins folder in `lite-xl`:

```ruby
# mod-version:3

core = Lua.require("core")
core.log_quiet.call("TEST")
old_log_quiet = core.log_quiet
core.log_quiet = Proc.new { |str, *args|
  puts "LOG QUIET: #{str} #{args}"
  old_log_quiet.call(str, *args)
}
```

You should have full interop with most `lite-xl` lua functions.
