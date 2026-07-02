#ifndef __MACROS_H
#define __MACROS_H

#ifdef API_EXPORTS
#define API __declspec(dllexport)
#else
#define API __declspec(dllimport)
#endif  // API_EXPORTS

#endif  // __MACROS_H
