-- Copyright 2010-present Greg Hurrell. All rights reserved.
-- Licensed under the terms of the BSD 2-clause license.

local ffi = require('ffi')

local commandt = {}

local chooser_buffer = nil
local chooser_selected_index = nil
local chooser_window = nil

local library = nil

-- require('wincent.commandt.finder') -- TODO: decide whether we need this, or
-- only scanners
local scanner = require('wincent.commandt.scanner')

-- print('scanner ' .. vim.inspect(scanner.buffer.get()))

library = {
  -- commandt_example_func_that_returns_int = function()
  --   if not loaded then
  --     library = library.load()
  --   end
  --
  --   return library.commandt_example_func_that_returns_int()
  -- end,
  --
  -- commandt_example_func_that_returns_str = function()
  --   if not loaded then
  --     library = library.load()
  --   end
  --
  --   return library.commandt_example_func_that_returns_str()
  -- end,

  -- TODO: just a demo; we might not end up exposing this function, as only
  -- matcher.c needs it (and anyway, it has a pointer-based out param, so we
  -- can't do that...
  commandt_calculate_match = function(str, needle, case_sensitive, always_show_dot_files, never_show_dot_files, recurse, needle_bitmask)--, haystack_bitmask)
    return library.load().commandt_calculate_match(str, needle, case_sensitive, always_show_dot_files, never_show_dot_files, recurse, needle_bitmask)--, haystack_bitmask)
  end,

  load = function ()
    local dirname = debug.getinfo(1).source:match('@?(.*/)')
    -- TODO: confirm that .so is auto-appended
    local extension = '.so' -- TODO: handle Windows .dll extension
    -- TODO: rename loaded (sounds like a boolean but it is the library
    library = ffi.load(dirname .. 'commandt' .. extension)

    ffi.cdef[[
      float commandt_calculate_match(
          const char *str,
          const char *needle,
          bool case_sensitive,
          bool always_show_dot_files,
          bool never_show_dot_files,
          bool recurse,
          long needle_bitmask
      );

      typedef struct {
          size_t count;
          const char **matches;
      } matches_t;

      matches_t commandt_sorted_matches_for(const char *needle);
    ]]
    -- TODO: avoid this; prefer to call destructor instead with ffi.gc and let
    -- C-side code do the freeing...
    -- void free(void *ptr);

    return library
  end,
}

-- TODO: make mappings configurable again
local mappings = {
  ['<C-j>'] = "<Cmd>lua require'wincent.commandt'.select_next()<CR>",
  ['<C-k>'] = "<Cmd>lua require'wincent.commandt'.select_previous()<CR>",
  ['<Down>'] = "<Cmd>lua require'wincent.commandt'.select_next()<CR>",
  ['<Up>'] = "<Cmd>lua require'wincent.commandt'.select_previous()<CR>",
}

local set_up_mappings = function()
  for lhs, rhs in pairs(mappings) do
    vim.api.nvim_set_keymap('c', lhs, rhs, {silent = true})
  end
end

local tear_down_mappings = function()
  for lhs, rhs in pairs(mappings) do
    if vim.fn.maparg(lhs, 'c') == rhs then
      vim.api.nvim_del_keymap('c', lhs)
    end
  end
end

commandt.buffer_finder = function()
  print(library.commandt_calculate_match('string', 'str', true, true, false, true, 0, nil))

  if true then
    return
  end

  -- print(library.commandt_example_func_that_returns_int())
  --
  -- print(ffi.string(library.commandt_example_func_that_returns_str()))
  --
  -- local t = {
  --     "one",
  --     "two",
  --     "three",
  --   }
  --   local ffi_t = ffi.new("const char *[4]", t);
  -- local flag = library.commandt_example_func_that_takes_a_table_of_strings(
  --   ffi.new("int", 3),
  --   -- 3 items + 1 NUL terminator
  --   ffi_t)
  --
  -- print('flag '..tonumber(flag))
  --
  -- local flag2 = library.commandt_example_func_that_takes_a_table_of_strings(
  --   ffi.new("int", 3),
  --   -- 3 items + 1 NUL terminator
  --   ffi_t -- this produces the same pointer
  --   -- ffi.new("const char *[4]", t) -- this is a diff value, producing a diff
  --   -- pointer
  --   )
  --
  -- print('flag2 '..tonumber(flag2))
  --
  -- -- and nil
  -- local flag3 = library.commandt_example_func_that_takes_a_table_of_strings(
  -- ffi.new("int", 0),
  -- ffi.new("const char *[1]", nil) -- does not wind up as NULL over there
  -- )
  -- print('flag3 '..tonumber(flag3))
  --
  -- local indices = library.commandt_example_func_that_returns_table_of_ints()
  --
  -- -- TODO copy this kind somewhere useful (ie. a cheatsheet)
  -- -- we can look up the size of the pointer to the array, but not
  -- -- the length of the array itself; it is terminated with a -1.
  -- -- print(ffi.sizeof(indices)) -- 8
  -- -- print(tostring(ffi.typeof(indices))) -- ctype<const int *>
  --
  -- local i = 0
  -- while true do
  --   local index = tonumber(indices[i])
  --   if index == -1 then
  --     break
  --   end
  --   print(index)
  --   i = i + 1
  -- end
  --
  -- local sorted = --ffi.gc(
  --   library.commandt_sorted_matches_for('some query')--,
  --   -- ffi.C.free
  -- --)
  -- -- (Note: don't free here, better to tell matcher/scanner to destruct and do its own free-ing)
  --
  -- -- tonumber() needed here because ULL (boxed)
  -- for i = 1, tonumber(sorted.count) do
  --   print(ffi.string(sorted.matches[i - 1]))
  -- end
end

commandt.cmdline_changed = function(char)
  if char == ':' then
    local line = vim.fn.getcmdline()
    local _, _, variant, query = string.find(line, '^%s*KommandT(%a*)%f[%A]%s*(.-)%s*$')
    if query ~= nil then
      if variant == '' or variant == 'Buffer' then
        set_up_mappings()
        local height = math.floor(vim.o.lines / 2) -- TODO make height somewhat dynamic
        local width = vim.o.columns
        if chooser_window == nil then
          chooser_buffer = vim.api.nvim_create_buf(false, true)
          chooser_window = vim.api.nvim_open_win(chooser_buffer, false, {
            col = 0,
            row = height,
            focusable = false,
            relative = 'editor',
            style = 'minimal',
            width = width,
            height = vim.o.lines - height - 2,
          })
          vim.api.nvim_win_set_option(chooser_window, 'wrap', false)
          vim.api.nvim_win_set_option(chooser_window, 'winhl', 'Normal:Question')
          vim.cmd('redraw')
        end
        return
      end
    end
  end
  tear_down_mappings()
end

commandt.cmdline_enter = function()
  chooser_selected_index = nil
end

commandt.cmdline_leave = function()
  if chooser_window ~= nil then
    vim.api.nvim_win_close(chooser_window, true)
    chooser_window = nil
  end
  tear_down_mappings()
end

commandt.file_finder = function(arg)
  local directory = vim.trim(arg)

  -- TODO: need to figure out what the semantics should be here as far as
  -- optional directory parameter goes
end

commandt.select_next = function()
end

commandt.select_previous = function()
end

return commandt
