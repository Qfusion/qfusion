#ifndef SVN_REVSTR
# if !defined( IDI_APPICON_VALUE )
// windows resource files apparently don't support __DATE__ and __TIME__
// so workaround this ( http://msdn2.microsoft.com/en-us/library/aa381032.aspx )
#  define SVN_REVSTR __DATE__ " " __TIME__
# endif
#endif
