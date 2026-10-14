/*
 * regex.cpp - regcomp() and friends for Windows, on std::regex, whose
 * "extended" grammar is POSIX's extended regular expressions.
 */
#include <cstring>
#include <regex>

#include "regex.h"

extern "C" int
regcomp(regex_t *preg, const char *pattern, int cflags)
{
	auto flags = (cflags & REG_EXTENDED) ? std::regex::extended
		: std::regex::basic;
	if (cflags & REG_ICASE)
		flags |= std::regex::icase;
	if (cflags & REG_NOSUB)
		flags |= std::regex::nosubs;
	try {
		auto *re = new std::regex(pattern, flags);
		preg->impl = re;
		preg->re_nsub = re->mark_count();
		return 0;
	} catch (const std::regex_error &) {
		preg->impl = nullptr;
		preg->re_nsub = 0;
		return REG_BADPAT;
	} catch (const std::bad_alloc &) {
		preg->impl = nullptr;
		return REG_ESPACE;
	}
}

extern "C" int
regexec(const regex_t *preg, const char *string, size_t nmatch,
		regmatch_t pmatch[], int eflags)
{
	(void)eflags;
	const auto *re = static_cast<const std::regex *>(preg->impl);
	std::cmatch m;
	if (!re || !std::regex_search(string, m, *re))
		return REG_NOMATCH;
	for (size_t i = 0; i < nmatch; ++i) {
		if (i < m.size() && m[i].matched) {
			pmatch[i].rm_so = m.position(i);
			pmatch[i].rm_eo = m.position(i) + m.length(i);
		} else {
			pmatch[i].rm_so = pmatch[i].rm_eo = -1;
		}
	}
	return 0;
}

extern "C" size_t
regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size)
{
	(void)preg;
	const char *msg = errcode == REG_NOMATCH ? "no match"
		: errcode == REG_ESPACE ? "out of memory"
		: "invalid regular expression";
	if (errbuf && errbuf_size) {
		std::strncpy(errbuf, msg, errbuf_size - 1);
		errbuf[errbuf_size - 1] = '\0';
	}
	return std::strlen(msg) + 1;
}

extern "C" void
regfree(regex_t *preg)
{
	delete static_cast<std::regex *>(preg->impl);
	preg->impl = nullptr;
}
