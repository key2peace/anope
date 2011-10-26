#include "module.h"
#include "../extra/sql.h"

class SQLCache : public Timer
{
	typedef std::map<Anope::string, time_t, std::less<ci::string> > cache_map;
	cache_map cache;
 public:

	SQLCache() : Timer(300, Anope::CurTime, true) { }

	bool Check(const Anope::string &item)
	{
		cache_map::iterator it = this->cache.find(item);
		if (it != this->cache.end() && Anope::CurTime - it->second < 5)
			return true;

		this->cache[item] = Anope::CurTime;
		return false;
	}

	void Tick(time_t)
	{
		for (cache_map::iterator it = this->cache.begin(), next_it; it != this->cache.end(); it = next_it)
		{
			next_it = it;
			++next_it;

			if (Anope::CurTime - it->second > 5)
				this->cache.erase(it);
		}
	}
};

static void ChanInfoUpdate(const Anope::string &name, const SQLResult &res)
{
	ChannelInfo *ci = cs_findchan(name);
	if (res.Rows() == 0)
	{
		delete ci;
		return;
	}

	try
	{
		if (!ci)
			ci = new ChannelInfo(name);
		ci->SetFounder(findcore(res.Get(0, "founder")));
		ci->successor = findcore(res.Get(0, "successor"));
		ci->desc = res.Get(0, "descr");
		ci->time_registered = convertTo<time_t>(res.Get(0, "time_registered"));
		ci->ClearFlags();
		ci->FromString(res.Get(0, "flags"));
		ci->bantype = convertTo<int>(res.Get(0, "bantype"));
		ci->memos.memomax = convertTo<unsigned>(res.Get(0, "memomax"));

		if (res.Get(0, "botnick").equals_cs(ci->bi ? ci->bi->nick : "") == false)
		{
			if (ci->bi)
				ci->bi->UnAssign(NULL, ci);
			BotInfo *bi = findbot(res.Get(0, "botnick"));
			if (bi)
				bi->Assign(NULL, ci);
		}

		ci->capsmin = convertTo<int16_t>(res.Get(0, "capsmin"));
		ci->capspercent = convertTo<int16_t>(res.Get(0, "capspercent"));
		ci->floodlines = convertTo<int16_t>(res.Get(0, "floodlines"));
		ci->floodsecs = convertTo<int16_t>(res.Get(0, "floodsecs"));
		ci->repeattimes = convertTo<int16_t>(res.Get(0, "repeattimes"));

		if (ci->c)
			check_modes(ci->c);
	}
	catch (const SQLException &ex)
	{
		Log() << ex.GetReason();
	}
	catch (const ConvertException &) { }
}

static void NickInfoUpdate(const Anope::string &name, const SQLResult &res)
{
	NickAlias *na = findnick(name);
	if (res.Rows() == 0)
	{
		delete na;
		return;
	}

	try
	{
		NickCore *nc = findcore(res.Get(0, "display"));
		if (!nc)
			return;
		if (!na)
			na = new NickAlias(name, nc);

		na->last_quit = res.Get(0, "last_quit");
		na->last_realname = res.Get(0, "last_realname");
		na->last_usermask = res.Get(0, "last_usermask");
		na->last_realhost = res.Get(0, "last_realhost");
		na->time_registered = convertTo<time_t>(res.Get(0, "time_registered"));
		na->last_seen = convertTo<time_t>(res.Get(0, "last_seen"));
		na->ClearFlags();
		na->FromString(res.Get(0, "flags"));

		if (na->nc != nc)
		{
			std::list<NickAlias *>::iterator it = std::find(na->nc->aliases.begin(), na->nc->aliases.end(), na);
			if (it != na->nc->aliases.end())
				na->nc->aliases.erase(it);

			na->nc = nc;
			na->nc->aliases.push_back(na);
		}
	}
	catch (const SQLException &ex)
	{
		Log() << ex.GetReason();
	}
	catch (const ConvertException &) { }
}

static void NickCoreUpdate(const Anope::string &name, const SQLResult &res)
{
	NickCore *nc = findcore(name);
	if (res.Rows() == 0)
	{
		delete nc;
		return;
	}

	try
	{
		if (!nc)
			nc = new NickCore(name);

		nc->pass = res.Get(0, "pass");
		nc->email = res.Get(0, "email");
		nc->greet = res.Get(0, "greet");
		nc->ClearFlags();
		nc->FromString(res.Get(0, "flags"));
		nc->language = res.Get(0, "language");
	}
	catch (const SQLException &ex)
	{
		Log() << ex.GetReason();
	}
	catch (const ConvertException &) { }
}

class MySQLLiveModule : public Module
{
	service_reference<SQLProvider, Base> SQL;

	SQLCache chan_cache, nick_cache, core_cache;

	SQLResult RunQuery(const SQLQuery &query)
	{
		if (!this->SQL)
			throw SQLException("Unable to locate SQL reference, is db_sql loaded and configured correctly?");

		SQLResult res = SQL->RunQuery(query);
		if (!res.GetError().empty())
			throw SQLException(res.GetError());
		return res;
	}

 public:
	MySQLLiveModule(const Anope::string &modname, const Anope::string &creator) :
		Module(modname, creator, DATABASE), SQL("")
	{
		Implementation i[] = { I_OnReload, I_OnFindChan, I_OnFindNick, I_OnFindCore, I_OnShutdown };
		ModuleManager::Attach(i, this, sizeof(i) / sizeof(Implementation));
	}

	void OnReload()
	{
		ConfigReader config;
		Anope::string engine = config.ReadValue("db_sql", "engine", "", 0);
		this->SQL = engine;
	}

	void OnShutdown()
	{
		Implementation i[] = { I_OnFindChan, I_OnFindNick, I_OnFindCore };
		for (size_t j = 0; j < 3; ++j)
			ModuleManager::Detach(i[j], this);
	}

	void OnFindChan(const Anope::string &chname)
	{
		if (chan_cache.Check(chname))
			return;

		try
		{
			SQLQuery query("SELECT * FROM `ChannelInfo` WHERE `name` = @name@");
			query.setValue("name", chname);
			SQLResult res = this->RunQuery(query);
			ChanInfoUpdate(chname, res);
		}
		catch (const SQLException &ex)
		{
			Log(LOG_DEBUG) << "OnFindChan: " << ex.GetReason();
		}
	}

	void OnFindNick(const Anope::string &nick)
	{
		if (nick_cache.Check(nick))
			return;

		try
		{
			SQLQuery query("SELECT * FROM `NickAlias` WHERE `nick` = @nick@");
			query.setValue("nick", nick);
			SQLResult res = this->RunQuery(query);
			NickInfoUpdate(nick, res);
		}
		catch (const SQLException &ex)
		{
			Log(LOG_DEBUG) << "OnFindNick: " << ex.GetReason();
		}
	}

	void OnFindCore(const Anope::string &nick)
	{
		if (core_cache.Check(nick))
			return;

		try
		{
			SQLQuery query("SELECT * FROM `NickCore` WHERE `display` = @display@");
			query.setValue("display", nick);
			SQLResult res = this->RunQuery(query);
			NickCoreUpdate(nick, res);
		}
		catch (const SQLException &ex)
		{
			Log(LOG_DEBUG) << "OnFindCore: " << ex.GetReason();
		}
	}
};

MODULE_INIT(MySQLLiveModule)