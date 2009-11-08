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

#define TO_COLLIDE 0 /* Collide the user with this nick */
#define TO_RELEASE 1 /* Release a collided nick */

class CommandNSLogout : public Command
{
 public:
	CommandNSLogout() : Command("LOGOUT", 0, 2)
	{
	}

	CommandReturn Execute(User *u, std::vector<ci::string> &params)
	{
		const char *nick = params.size() ? params[0].c_str() : NULL;
		ci::string param = params.size() > 1 ? params[1] : "";
		User *u2;
		NickAlias *na;

		if (!u->nc->IsServicesOper() && nick)
			this->OnSyntaxError(u);
		else if (!(u2 = (nick ? finduser(nick) : u)))
			notice_lang(s_NickServ, u, NICK_X_NOT_IN_USE, nick);
		else if (nick && u2->nc && !u2->nc->IsServicesOper())
			notice_lang(s_NickServ, u, NICK_LOGOUT_SERVICESADMIN, nick);
		else
		{
			na = findnick(u2->nick);

			if (nick && !param.empty() && param == "REVALIDATE")
			{
				cancel_user(u2);
				validate_user(u2);
			}

			if (ircd->modeonunreg)
				common_svsmode(u2, ircd->modeonunreg, "1");

			u2->isSuperAdmin = 0; /* Dont let people logout and remain a SuperAdmin */
			alog("%s: %s!%s@%s logged out nickname %s", s_NickServ, u->nick, u->GetIdent().c_str(), u->host, u2->nick);

			/* Remove founder status from this user in all channels */
			if (nick)
				notice_lang(s_NickServ, u, NICK_LOGOUT_X_SUCCEEDED, nick);
			else
				notice_lang(s_NickServ, u, NICK_LOGOUT_SUCCEEDED);

			/* Clear any timers again */
			if (na && u->nc->HasFlag(NI_KILLPROTECT))
				del_ns_timeout(na, TO_COLLIDE);

			ircdproto->SendAccountLogout(u2, u2->nc);

			u2->nc = NULL;

			/* Send out an event */
			FOREACH_MOD(I_OnNickLogout, OnNickLogout(u2));
		}
		return MOD_CONT;
	}

	bool OnHelp(User *u, const ci::string &subcommand)
	{
		if (u->nc && u->nc->IsServicesOper())
			notice_help(s_NickServ, u, NICK_SERVADMIN_HELP_LOGOUT);
		else
			notice_help(s_NickServ, u, NICK_HELP_LOGOUT);

		return true;
	}

	void OnSyntaxError(User *u)
	{
		syntax_error(s_NickServ, u, "LOGOUT", NICK_LOGOUT_SYNTAX);
	}
};

class NSLogout : public Module
{
 public:
	NSLogout(const std::string &modname, const std::string &creator) : Module(modname, creator)
	{
		this->SetAuthor("Anope");
		this->SetVersion("$Id$");
		this->SetType(CORE);

		this->AddCommand(NICKSERV, new CommandNSLogout());
	}
	void NickServHelp(User *u)
	{
		notice_lang(s_NickServ, u, NICK_HELP_CMD_LOGOUT);
	}
};

MODULE_INIT(NSLogout)
