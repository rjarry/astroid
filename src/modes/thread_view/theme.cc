# include "theme.hh"

# include <atomic>
# include <iostream>
# include <fstream>
# include <boost/filesystem.hpp>

# include "astroid.hh"
# include "config.hh"
# include "utils/resource.hh"

# ifndef DISABLE_LIBSASS
# include <boost/regex.hpp>
# include "csscolorparser.hpp"

# ifdef SASSCTX_CONTEXT_H
  # include <sass/context.h>
# elif SASSCTX_SASS_CONTEXT_H
  # include <sass_context.h>
# endif

# endif


using namespace std;
using namespace boost::filesystem;
using namespace boost::property_tree;

namespace Astroid {
  std::atomic<bool> Theme::theme_loaded (false);
  const char * Theme::thread_view_html_f = "ui/thread-view.html";
# ifndef DISABLE_LIBSASS
  const char * Theme::thread_view_scss_f  = "ui/thread-view.scss";
# else
  const char * Theme::thread_view_css_f  = "ui/thread-view.css";
# endif
  ustring Theme::thread_view_html;
  ustring Theme::thread_view_css;

  Theme::Theme () {
    using bfs::path;
    using std::endl;
    LOG (debug) << "theme: loading..";

    /* load html and css (from scss) */
    if (!theme_loaded) {
      path tv_html = Resource (true, thread_view_html_f).get_path ();

      if (!check_theme_version (tv_html)) {

        LOG (error) << "tv: html file version does not match!";

      }

# ifndef DISABLE_LIBSASS
      path tv_scss = Resource (true, thread_view_scss_f).get_path ();
      if (!check_theme_version (tv_scss)) {

        LOG (error) << "tv: scss file version does not match!";

      }
# else
      path tv_css = Resource (true, thread_view_css_f).get_path ();

      if (!check_theme_version (tv_css)) {

        LOG (error) << "tv: css file version does not match!";

      }
# endif

      {
        std::ifstream tv_html_f (tv_html.c_str());
        std::istreambuf_iterator<char> eos; // default is eos
        std::istreambuf_iterator<char> tv_iit (tv_html_f);

        thread_view_html.append (tv_iit, eos);
        tv_html_f.close ();
      }

# ifndef DISABLE_LIBSASS
      thread_view_css = process_scss (tv_scss.c_str ());
# else
      {
        std::ifstream tv_css_f (tv_css.c_str());
        std::istreambuf_iterator<char> eos; // default is eos
        std::istreambuf_iterator<char> tv_iit (tv_css_f);

        thread_view_css.append (tv_iit, eos);
        tv_css_f.close ();
      }
# endif

      theme_loaded = true;
    }
  }

# ifndef DISABLE_LIBSASS
  static union Sass_Value *config_cb(const union Sass_Value *args,
                                     Sass_Function_Entry cb,
                                     struct Sass_Compiler* compiler) {
    (void) cb;
    (void) compiler;

    LOG (debug) << "theme: astroid-config: called";

    if (!(sass_value_is_list(args) && sass_list_get_length(args) == 2)) {
      LOG (error) << "theme: astroid-config: cannot parse arguments";
      return sass_make_null();
    }

    auto *conf = sass_list_get_value(args, 0);
    auto *def_value = sass_list_get_value(args, 1);
    auto *c = sass_string_get_value(sass_value_stringify(conf, false, 1));
    ptree node;

    try {
      node = astroid->config().get_child(c);
    } catch (ptree_bad_path) {
      return def_value;
    }

    try {
      // try to convert to boolean
      bool value = node.get_value<bool>();
      return sass_make_boolean(value);
    } catch (ptree_bad_data) {
      // pass
    }

    string s_value;

    try {
      s_value = node.get_value<string>();
    } catch (ptree_bad_data) {
      return def_value;
    }

    // try to convert to number + unit
    static boost::regex exp("\\s*(\\d+(?:\\.\\d+))([a-z%]*)\\s*");
    boost::smatch match;
    if (boost::regex_match(s_value, match, exp, boost::match_extra)) {
      double number = boost::lexical_cast<double>(match[1]);
      return sass_make_number(number, match[2].str().c_str());
    }

    // try to convert to css color
    boost::optional<CSSColorParser::Color> color;
    color = CSSColorParser::parse(s_value);
    if (color) {
      return sass_make_color(color->r, color->g, color->b, color->a);
    }

    return sass_make_string(s_value.c_str());
  }

  ustring Theme::process_scss (const char * scsspath) {
    /* - https://github.com/sass/libsass/blob/master/docs/api-doc.md
     * - https://github.com/sass/libsass/blob/master/docs/api-context-example.md
     */
    using std::endl;

    LOG (info) << "theme: processing: " << scsspath;

    struct Sass_File_Context* file_ctx = sass_make_file_context(scsspath);
    struct Sass_Options* options = sass_file_context_get_options(file_ctx);
    struct Sass_Context* context = sass_file_context_get_context(file_ctx);
    sass_option_set_precision(options, 1);
    sass_option_set_source_comments(options, true);
    // allocate a custom function caller
    Sass_Function_Entry fn_config = sass_make_function(
            "astroid-config($conf, $default: '')", config_cb, NULL);
    Sass_Function_List fn_list = sass_make_function_list(1);
    sass_function_set_list_entry(fn_list, 0, fn_config);
    sass_option_set_c_functions(options, fn_list);

    int status = sass_compile_file_context (file_ctx);

    if (status != 0) {
      const char * err = sass_context_get_error_message (context);
      ustring erru (err);
      LOG (error) << "theme: error processing: " << erru;

      throw runtime_error (
          ustring::compose ("theme: could not process scss: %1", erru).c_str ());
    }

    const char * output = sass_context_get_output_string(context);
    ustring output_str(output);
    sass_delete_file_context (file_ctx);

    return output_str;
  }
# endif

  bool Theme::check_theme_version (bfs::path p) {
    /* check version found in first line in file */

    std::ifstream f (p.c_str ());

    ustring vline;
    int version;
    f >> vline >> vline >> version;

    LOG (debug) << "tv: testing version: " << version;

    f.close ();

    return (version == THEME_VERSION);
  }
}

