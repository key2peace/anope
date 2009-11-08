/* NickServ core functions
 *
 * (C) 2003-2009 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 *
 * $Id$
 *
 */
/*************************************************************************/

#include "module.h"

class CommandNSAList : public Command
{
 public:
	CommandNSAList() : Command("ALIST", 0, 2)
	{
	}

	CommandReturn Execute(User *u, std::vector<ci::string> &params)
	{
		/*
		 * List the channels that the given nickname has access on
		 *
		 * /ns ALIST [level]
		 * /ns ALIST [nickname] [level]
		 *
		 * -jester
		 */

		const char *nick = NULL;

		NickAlias *na;

		int min_level = 0;
		int is_servadmin = u->nc->IsServicesOper();
		unsigned lev_param = 0;

		if (!is_servadmin)
			/* Non service admins can only see their own levels */
			na = findnick(u->nc->display);
		else
		{
			/* Services admins can request ALIST on nicks.
			 * The first argument for service admins must
			 * always be a nickname.
			 */
			nick = params.size() ? params[0].c_str() : NULL;
			lev_param = 1;

			/* If an argument was passed, use it as the nick to see levels
			 * for, else check levels for the user calling the command */
			if (nick)
				na = findnick(nick);
			else
				na = findnick(u->nick);
		}

		/* If available, get level from arguments */
		ci::string lev = params.size() > lev_param ? params[lev_param] : "";

		/* if a level was given, make sure it's an int for later */
		if (!lev.empty())
		{
			if (lev == "FOUNDER")
				min_level = ACCESS_FOUNDER;
			else if (lev == "SOP")
				min_level = ACCESS_SOP;
			else if (lev == "AOP")
				min_level = ACCESS_AOP;
			else if (lev == "HOP")
				min_level = ACCESS_HOP;
			else if (lev == "VOP")
				min_level = ACCESS_VOP;
			else
				min_level = atoi(lev.c_str());
		}

		if (!na)
			notice_lang(s_NickServ, u, NICK_X_NOT_REGISTERED, nick);
		else if (na->HasFlag(NS_FORBIDDEN))
			notice_lang(s_NickServ, u, NICK_X_FORBIDDEN, na->nick);
		else if (min_level <= ACCESS_INVALID || min_level > ACCESS_FOUNDER)
			notice_lang(s_NickServ, u, CHAN_ACCESS_LEVEL_RANGE, ACCESS_INVALID + 1, ACCESS_FOUNDER - 1);
		else
		{
			int i, level;
			int chan_count = 0;
			int match_count = 0;
			ChannelInfo *ci;

			notice_lang(s_NickServ, u, is_servadmin ? NICK_ALIST_HEADER_X : NICK_ALIST_HEADER, na->nick);

			for (i = 0; i < 256; ++i)
			{
				for (ci = chanlists[i]; ci; ci = ci->next)
				{
					if ((level = get_access_level(ci, na)))
					{
						++chan_count;

						if (min_level > level)
							continue;

						++match_count;

						if ((ci->HasFlag(CI_XOP)) || level == ACCESS_FOUNDER)
						{
							const char *xop;

							xop = get_xop_level(level);

							notice_lang(s_NickServ, u, NICK_ALIST_XOP_FORMAT, match_count, ci->HasFlag(CI_NO_EXPIRE) ? '!' : ' ', ci->name, xop, ci->desc ? ci->desc : "");
						}
						else
							notice_lang(s_NickServ, u, NICK_ALIST_ACCESS_FORMAT, match_count, ci->HasFlag(CI_NO_EXPIRE) ? '!' : ' ', ci->name, level, ci->desc ? ci->desc : "");
					}
				}
			}

			notice_lang(s_NickServ, u, NICK_ALIST_FOOTER, match_count, chan_count);
		}
		return MOD_CONT;
	}

	bool OnHelp(User *u, const ci::string &subcommand)
	{
		if (u->nc && u->nc->IsServicesOper())
			notice_help(s_NickServ, u, NICK_SERVADMIN_HELP_ALIST);
		else
			notice_help(s_NickServ, u, NICK_HELP_ALIST);

		return true;
	}
};

class NSAList : public Module
{
 public:
	NSAList(const std::string &modname, const std::string &creator) : Module(modname, creator)
	{
		this->SetAuthor("Anope");
		this->SetVersion("$Id$");
		this->SetType(CORE);

		this->AddCommand(NICKSERV, new CommandNSAList());
	}
	void NickServHelp(User *u)
	{
		notice_lang(s_NickServ, u, NICK_HELP_CMD_ALIST);
	}
};

MODULE_INIT(NSAList)
