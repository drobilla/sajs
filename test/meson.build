# Copyright 2021-2024 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

############
# Metadata #
############

if get_option('lint')
  python_script_names = ['test_parse.py', 'test_thru.py']
  python_scripts = files(python_script_names)

  # Check licensing metadata
  reuse = find_program('reuse', required: false)
  if reuse.found()
    test(
      'REUSE',
      reuse,
      args: ['--root', sajs_src_root, 'lint'],
      suite: 'data',
    )
  endif

  # Check script formatting
  black = find_program('black', required: false)
  if black.found()
    black_opts = ['--check', '-q', '-l', '79']
    foreach name : python_script_names
      test(
        name.underscorify(),
        black,
        args: black_opts + files(name),
        suite: 'scripts',
      )
    endforeach
  endif

  # Check scripts for errors with flake8
  flake8 = find_program('flake8', required: false)
  if flake8.found()
    test('flake8', flake8, args: python_scripts, suite: 'scripts')
  endif

  # Check scripts for errors with pylint
  pylint = find_program('pylint', required: false)
  if pylint.found()
    pymod = import('python')
    plot_py = pymod.find_installation(
      'python3',
      modules: ['matplotlib'],
      required: false,
    )

    pylint_args = ['--disable', 'bad-option-value']
    test('pylint', pylint, args: pylint_args + python_scripts, suite: 'scripts')
  endif
endif

##########
# Header #
##########

# Set up warning suppressions for header test
header_suppressions = []
if cc.get_id() in ['clang', 'emscripten']
  if warning_level == 'everything'
    header_suppressions += ['-Wno-padded']
    if not meson.is_cross_build()
      header_suppressions += ['-Wno-poison-system-directories']
    endif
  endif
  if warning_level in ['everything', '3']
    header_suppressions += ['-Wno-nullability-extension']
  endif
elif cc.get_id() == 'gcc'
  if warning_level == 'everything'
    header_suppressions += ['-Wno-padded']
  endif
endif

# Test that header builds with stricter warnings
test(
  'test_header',
  executable(
    'test_header',
    files('headers/test_header.c'),
    c_args: header_suppressions,
    dependencies: sajs_dep,
    include_directories: include_dirs,
  ),
)

##############
# Unit Tests #
##############

test_init = executable(
  'test_init',
  files('test_init.c'),
  c_args: c_suppressions + program_c_args,
  link_args: program_link_args,
  dependencies: [sajs_dep],
)

test('init', test_init, suite: 'unit')

#################
# Utility Tests #
#################

test('version', sajs_pipe, args: ['-V'], suite: 'args', timeout: 5)
test('help', sajs_pipe, args: ['-h'], suite: 'args', timeout: 5)

if host_machine.cpu_family() not in ['wasm32', 'wasm64']
  test_file = files('../test/pretty/empty_array.json')
  test('tiny_k', sajs_pipe, args: ['-k', '2', test_file], suite: 'args')
endif

test('bad_arg', sajs_pipe, args: ['-b'], should_fail: true, suite: 'args')
test('bad_k', sajs_pipe, args: ['-k', 'b'], should_fail: true, suite: 'args')
test('missing_k', sajs_pipe, args: ['-k'], should_fail: true, suite: 'args')
test('zero_k', sajs_pipe, args: ['-k', '0'], should_fail: true, suite: 'args')

test(
  'huge_k',
  sajs_pipe,
  args: ['-k', '9223372036854775806'],
  should_fail: true,
  suite: 'args',
)

test(
  'missing_o',
  sajs_pipe,
  args: ['-o'],
  should_fail: true,
  suite: 'args',
)

test(
  'missing_input',
  sajs_pipe,
  args: ['/does/not/exist'],
  should_fail: true,
  suite: 'args',
)

test(
  'missing_output',
  sajs_pipe,
  args: ['-o', '/does/not/exist'],
  should_fail: true,
  suite: 'args',
)

#############################
# Data-Driven Reading Tests #
#############################

test_parse = find_program('test_parse.py')
test_thru = find_program('test_thru.py')

perfect_tests = [
  'i_number_double_huge_neg_exp',
  'i_number_huge_exp',
  'i_number_neg_int_huge_exp',
  'i_number_pos_double_huge_exp',
  'i_number_real_neg_overflow',
  'i_number_real_pos_overflow',
  'i_number_real_underflow',
  'i_number_too_big_neg_int',
  'i_number_too_big_pos_int',
  'i_number_very_big_negative_int',
  'i_structure_500_nested_arrays',
  'y_array_empty',
  'y_array_empty-string',
  'y_array_ending_with_newline',
  'y_array_false',
  'y_array_null',
  'y_array_with_several_null',
  'y_number',
  'y_number_0e+1',
  'y_number_0e1',
  'y_number_int_with_exp',
  'y_number_minus_zero',
  'y_number_negative_int',
  'y_number_negative_one',
  'y_number_negative_zero',
  'y_number_real_capital_e',
  'y_number_real_capital_e_neg_exp',
  'y_number_real_capital_e_pos_exp',
  'y_number_real_exponent',
  'y_number_real_fraction_exponent',
  'y_number_real_neg_exp',
  'y_number_real_pos_exponent',
  'y_number_simple_int',
  'y_number_simple_real',
  'y_object_basic',
  'y_object_duplicated_key',
  'y_object_duplicated_key_and_value',
  'y_object_empty',
  'y_object_empty_key',
  'y_object_simple',
  'y_string_backslash_and_u_escaped_zero',
  'y_string_backslash_doublequotes',
  'y_string_comments',
  'y_string_double_escape_a',
  'y_string_double_escape_n',
  'y_string_escaped_control_character',
  'y_string_in_array',
  'y_string_nonCharacterInUTF-8_U+10FFFF',
  'y_string_nonCharacterInUTF-8_U+FFFF',
  'y_string_null_escape',
  'y_string_pi',
  'y_string_reservedCharacterInUTF-8_U+1BFFF',
  'y_string_simple_ascii',
  'y_string_space',
  'y_string_u+2028_line_sep',
  'y_string_u+2029_par_sep',
  'y_string_unescaped_char_delete',
  'y_string_unicode_2',
  'y_string_utf8',
  'y_string_with_del_character',
  'y_structure_lonely_false',
  'y_structure_lonely_int',
  'y_structure_lonely_negative_real',
  'y_structure_lonely_null',
  'y_structure_lonely_string',
  'y_structure_lonely_true',
  'y_structure_string_empty',
  'y_structure_true_in_array',
]

good_tests = [
  'i_string_UTF-8_invalid_sequence',
  'i_string_UTF8_surrogate_U+D800',
  'i_string_invalid_utf-8',
  'i_string_iso_latin_1',
  'i_string_lone_utf8_continuation_byte',
  'i_string_not_in_unicode_range',
  'i_string_overlong_sequence_2_bytes',
  'i_string_overlong_sequence_6_bytes',
  'i_string_overlong_sequence_6_bytes_null',
  'i_string_truncated-utf-8',
  'y_array_arraysWithSpaces',
  'y_array_heterogeneous',
  'y_array_with_1_and_newline',
  'y_array_with_leading_space',
  'y_array_with_trailing_space',
  'y_number_after_space',
  'y_number_double_close_to_zero',
  'y_object',
  'y_object_escaped_null_in_key',
  'y_object_extreme_numbers',
  'y_object_long_strings',
  'y_object_string_unicode',
  'y_object_with_newlines',
  'y_string_1_2_3_bytes_UTF-8_sequences',
  'y_string_accepted_surrogate_pair',
  'y_string_accepted_surrogate_pairs',
  'y_string_allowed_escapes',
  'y_string_escaped_noncharacter',
  'y_string_in_array_with_leading_space',
  'y_string_last_surrogates_1_and_2',
  'y_string_nbsp_uescaped',
  'y_string_one-byte-utf-8',
  'y_string_surrogates_U+1D11E_MUSICAL_SYMBOL_G_CLEF',
  'y_string_three-byte-utf-8',
  'y_string_two-byte-utf-8',
  'y_string_uEscape',
  'y_string_uescaped_newline',
  'y_string_unicode',
  'y_string_unicodeEscapedBackslash',
  'y_string_unicode_U+10FFFE_nonchar',
  'y_string_unicode_U+1FFFE_nonchar',
  'y_string_unicode_U+200B_ZERO_WIDTH_SPACE',
  'y_string_unicode_U+2064_invisible_plus',
  'y_string_unicode_U+FDD0_nonchar',
  'y_string_unicode_U+FFFE_nonchar',
  'y_string_unicode_escaped_double_quote',
  'y_structure_trailing_newline',
  'y_structure_whitespace_array',
]

bad_tests = [
  'i_object_key_lone_2nd_surrogate',
  'i_string_1st_surrogate_but_2nd_missing',
  'i_string_1st_valid_surrogate_2nd_invalid',
  'i_string_UTF-16LE_with_BOM',
  'i_string_incomplete_surrogate_and_escape_valid',
  'i_string_incomplete_surrogate_pair',
  'i_string_incomplete_surrogates_escape_valid',
  'i_string_invalid_lonely_surrogate',
  'i_string_invalid_surrogate',
  'i_string_inverted_surrogates_U+1D11E',
  'i_string_lone_second_surrogate',
  'i_string_utf16BE_no_BOM',
  'i_string_utf16LE_no_BOM',
  'i_structure_UTF-8_BOM_empty_object',
  'n_array_1_true_without_comma',
  'n_array_a_invalid_utf8',
  'n_array_colon_instead_of_comma',
  'n_array_comma_after_close',
  'n_array_comma_and_number',
  'n_array_double_comma',
  'n_array_double_extra_comma',
  'n_array_extra_close',
  'n_array_extra_comma',
  'n_array_incomplete',
  'n_array_incomplete_invalid_value',
  'n_array_inner_array_no_comma',
  'n_array_invalid_utf8',
  'n_array_items_separated_by_semicolon',
  'n_array_just_comma',
  'n_array_just_minus',
  'n_array_missing_value',
  'n_array_newlines_unclosed',
  'n_array_number_and_comma',
  'n_array_number_and_several_commas',
  'n_array_spaces_vertical_tab_formfeed',
  'n_array_star_inside',
  'n_array_unclosed',
  'n_array_unclosed_trailing_comma',
  'n_array_unclosed_with_new_lines',
  'n_array_unclosed_with_object_inside',
  'n_incomplete_false',
  'n_incomplete_null',
  'n_incomplete_true',
  'n_multidigit_number_then_00',
  'n_number_++',
  'n_number_+1',
  'n_number_+Inf',
  'n_number_-01',
  'n_number_-1.0.',
  'n_number_-2.',
  'n_number_-NaN',
  'n_number_.-1',
  'n_number_.2e-3',
  'n_number_0.1.2',
  'n_number_0.3e',
  'n_number_0.3e+',
  'n_number_0.e1',
  'n_number_0_capital_E',
  'n_number_0_capital_E+',
  'n_number_0e',
  'n_number_0e+',
  'n_number_1.0e',
  'n_number_1.0e+',
  'n_number_1.0e-',
  'n_number_1_000',
  'n_number_1eE2',
  'n_number_2.e+3',
  'n_number_2.e-3',
  'n_number_2.e3',
  'n_number_9.e+',
  'n_number_Inf',
  'n_number_NaN',
  'n_number_U+FF11_fullwidth_digit_one',
  'n_number_expression',
  'n_number_hex_1_digit',
  'n_number_hex_2_digits',
  'n_number_infinity',
  'n_number_invalid+-',
  'n_number_invalid-negative-real',
  'n_number_invalid-utf-8-in-bigger-int',
  'n_number_invalid-utf-8-in-exponent',
  'n_number_invalid-utf-8-in-int',
  'n_number_minus_infinity',
  'n_number_minus_sign_with_trailing_garbage',
  'n_number_minus_space_1',
  'n_number_neg_int_starting_with_zero',
  'n_number_neg_real_without_int_part',
  'n_number_neg_with_garbage_at_end',
  'n_number_real_garbage_after_e',
  'n_number_real_with_invalid_utf8_after_e',
  'n_number_real_without_fractional_part',
  'n_number_starting_with_dot',
  'n_number_with_alpha',
  'n_number_with_alpha_char',
  'n_number_with_leading_zero',
  'n_object_bad_value',
  'n_object_bracket_key',
  'n_object_comma_instead_of_colon',
  'n_object_double_colon',
  'n_object_emoji',
  'n_object_garbage_at_end',
  'n_object_key_with_single_quotes',
  'n_object_lone_continuation_byte_in_key_and_trailing_comma',
  'n_object_missing_colon',
  'n_object_missing_key',
  'n_object_missing_semicolon',
  'n_object_missing_value',
  'n_object_no-colon',
  'n_object_non_string_key',
  'n_object_non_string_key_but_huge_number_instead',
  'n_object_repeated_null_null',
  'n_object_several_trailing_commas',
  'n_object_single_quote',
  'n_object_trailing_comma',
  'n_object_trailing_comment',
  'n_object_trailing_comment_open',
  'n_object_trailing_comment_slash_open',
  'n_object_trailing_comment_slash_open_incomplete',
  'n_object_two_commas_in_a_row',
  'n_object_unquoted_key',
  'n_object_unterminated-value',
  'n_object_with_single_string',
  'n_object_with_trailing_garbage',
  'n_single_space',
  'n_string_1_surrogate_then_escape',
  'n_string_1_surrogate_then_escape_u',
  'n_string_1_surrogate_then_escape_u1',
  'n_string_1_surrogate_then_escape_u1x',
  'n_string_accentuated_char_no_quotes',
  'n_string_backslash_00',
  'n_string_escape_x',
  'n_string_escaped_backslash_bad',
  'n_string_escaped_ctrl_char_tab',
  'n_string_escaped_emoji',
  'n_string_incomplete_escape',
  'n_string_incomplete_escaped_character',
  'n_string_incomplete_surrogate',
  'n_string_incomplete_surrogate_escape_invalid',
  'n_string_invalid-utf-8-in-escape',
  'n_string_invalid_backslash_esc',
  'n_string_invalid_unicode_escape',
  'n_string_invalid_utf8_after_escape',
  'n_string_leading_uescaped_thinspace',
  'n_string_no_quotes_with_bad_escape',
  'n_string_single_doublequote',
  'n_string_single_quote',
  'n_string_single_string_no_double_quotes',
  'n_string_start_escape_unclosed',
  'n_string_unescaped_ctrl_char',
  'n_string_unescaped_newline',
  'n_string_unescaped_tab',
  'n_string_unicode_CapitalU',
  'n_string_with_trailing_garbage',
  'n_structure_100000_opening_arrays',
  'n_structure_U+2060_word_joined',
  'n_structure_UTF8_BOM_no_data',
  'n_structure_angle_bracket_.',
  'n_structure_angle_bracket_null',
  'n_structure_array_trailing_garbage',
  'n_structure_array_with_extra_array_close',
  'n_structure_array_with_unclosed_string',
  'n_structure_ascii-unicode-identifier',
  'n_structure_capitalized_True',
  'n_structure_close_unopened_array',
  'n_structure_comma_instead_of_closing_brace',
  'n_structure_double_array',
  'n_structure_end_array',
  'n_structure_incomplete_UTF8_BOM',
  'n_structure_lone-invalid-utf-8',
  'n_structure_lone-open-bracket',
  'n_structure_no_data',
  'n_structure_null-byte-outside-string',
  'n_structure_number_with_trailing_garbage',
  'n_structure_object_followed_by_closing_object',
  'n_structure_object_unclosed_no_value',
  'n_structure_object_with_comment',
  'n_structure_object_with_trailing_garbage',
  'n_structure_open_array_apostrophe',
  'n_structure_open_array_comma',
  'n_structure_open_array_object',
  'n_structure_open_array_open_object',
  'n_structure_open_array_open_string',
  'n_structure_open_array_string',
  'n_structure_open_object',
  'n_structure_open_object_close_array',
  'n_structure_open_object_comma',
  'n_structure_open_object_open_array',
  'n_structure_open_object_open_string',
  'n_structure_open_object_string_with_apostrophes',
  'n_structure_open_open',
  'n_structure_single_eacute',
  'n_structure_single_star',
  'n_structure_trailing_#',
  'n_structure_uescaped_LF_before_string',
  'n_structure_unclosed_array',
  'n_structure_unclosed_array_partial_null',
  'n_structure_unclosed_array_unfinished_false',
  'n_structure_unclosed_array_unfinished_true',
  'n_structure_unclosed_object',
  'n_structure_unicode-identifier',
  'n_structure_whitespace_U+2060_word_joiner',
  'n_structure_whitespace_formfeed',
]

test_script_args = ['--tool', sajs_pipe]

foreach name : perfect_tests
  input = files('JSONTestSuite' / 'test_parsing' / name + '.json')

  test(
    name,
    test_parse,
    args: test_script_args + [input],
    suite: 'good',
    timeout: 5,
  )

  test(
    name + '_thru',
    test_thru,
    args: test_script_args + ['--terse', input],
    suite: 'thru',
    timeout: 5,
  )
endforeach

foreach name : good_tests
  input = files('JSONTestSuite' / 'test_parsing' / name + '.json')
  test(
    name,
    test_parse,
    args: test_script_args + [input],
    suite: 'good',
    timeout: 5,
  )
endforeach

foreach name : bad_tests
  input = files('JSONTestSuite' / 'test_parsing' / name + '.json')
  test(
    name,
    test_parse,
    args: test_script_args + [input],
    should_fail: true,
    suite: 'bad',
    timeout: 5,
  )
endforeach

#############################
# Data-Driven Writing Tests #
#############################

pretty_tests = [
  'array_in_array',
  'array_in_object',
  'empty_array',
  'empty_array_in_array',
  'empty_array_in_object',
  'empty_object',
  'empty_object_in_array',
  'empty_object_in_object',
  'object_in_object',
  'object_in_array',
  'simple_array',
  'simple_object',
]

foreach name : pretty_tests
  input = files('pretty' / name + '.json')

  test(
    name,
    test_thru,
    args: test_script_args + [input],
    suite: 'pretty',
    timeout: 5,
  )
endforeach
