/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "xline.h"

#include "treesocket.h"
#include "treeserver.h"
#include "utils.h"
#include "link.h"
#include "main.h"

std::string TreeSocket::MyModules(int filter)
{
	std::vector<std::string> modlist = ServerInstance->Modules->GetAllModuleNames(filter);

	if (filter == VF_COMMON && proto_version != ProtocolVersion)
		CompatAddModules(modlist);

	std::string capabilities;
	sort(modlist.begin(),modlist.end());
	for (unsigned int i = 0; i < modlist.size(); i++)
	{
		if (i)
			capabilities.push_back(proto_version > 1201 ? ' ' : ',');
		capabilities.append(modlist[i]);
		Module* m = ServerInstance->Modules->Find(modlist[i]);
		if (m && proto_version > 1201)
		{
			Version v = m->GetVersion();
			if (!v.link_data.empty())
			{
				capabilities.push_back('=');
				capabilities.append(v.link_data);
			}
		}
	}
	return capabilities;
}

static std::string BuildModeList(ModeType type, int proto_version)
{
	std::vector<std::string> modes;
	for(ModeIDIter id; id; id++)
	{
		ModeHandler* mh = ServerInstance->Modes->FindMode(id);
		if (mh && mh->GetModeType() == type)
		{
			std::string mdesc = mh->name;
			mdesc.push_back('=');
			if (mh->GetPrefix())
				mdesc.push_back(mh->GetPrefix());
			if (mh->GetModeChar())
				mdesc.push_back(mh->GetModeChar());
			modes.push_back(mdesc);
		}
	}
	if (type == MODETYPE_CHANNEL && proto_version == 1202 && ServerInstance->Config->NameOnlyModes)
		modes.push_back("namebase=Z");
	sort(modes.begin(), modes.end());
	irc::stringjoiner line(" ", modes, 0, modes.size() - 1);
	return line.GetJoined();
}

void TreeSocket::SendCapabilities(int phase)
{
	if (capab->capab_phase >= phase)
		return;

	if (capab->capab_phase < 1 && phase >= 1)
		WriteLine("CAPAB START " + ConvToStr(ProtocolVersion));

	capab->capab_phase = phase;
	if (phase < 2)
		return;

	char sep = proto_version > 1201 ? ' ' : ',';
	irc::sepstream modulelist(MyModules(VF_COMMON), sep);
	irc::sepstream optmodulelist(MyModules(VF_OPTCOMMON), sep);
	/* Send module names, split at 509 length */
	std::string item;
	std::string line = "CAPAB MODULES :";
	while (modulelist.GetToken(item))
	{
		if (line.length() + item.length() + 1 > 509)
		{
			this->WriteLine(line);
			line = "CAPAB MODULES :";
		}

		if (line != "CAPAB MODULES :")
			line.push_back(sep);

		line.append(item);
	}
	if (line != "CAPAB MODULES :")
		this->WriteLine(line);

	line = "CAPAB MODSUPPORT :";
	while (optmodulelist.GetToken(item))
	{
		if (line.length() + item.length() + 1 > 509)
		{
			this->WriteLine(line);
			line = "CAPAB MODSUPPORT :";
		}

		if (line != "CAPAB MODSUPPORT :")
			line.push_back(sep);

		line.append(item);
	}
	if (line != "CAPAB MODSUPPORT :")
		this->WriteLine(line);

	WriteLine("CAPAB CHANMODES :" + BuildModeList(MODETYPE_CHANNEL, proto_version));
	WriteLine("CAPAB USERMODES :" + BuildModeList(MODETYPE_USER, proto_version));

	std::string extra;
	/* Do we have sha256 available? If so, we send a challenge */
	if (Utils->ChallengeResponse && (ServerInstance->Modules->Find("m_sha256.so")))
	{
		SetOurChallenge(ServerInstance->GenRandomStr(20));
		extra = " CHALLENGE=" + this->GetOurChallenge();
	}
	if (proto_version < 1202)
		extra += ServerInstance->Modes->FindMode('h', MODETYPE_CHANNEL) ? " HALFOP=1" : " HALFOP=0";

	this->WriteLine("CAPAB CAPABILITIES " /* Preprocessor does this one. */
			":NICKMAX="+ConvToStr(ServerInstance->Config->Limits.NickMax)+
			" CHANMAX="+ConvToStr(ServerInstance->Config->Limits.ChanMax)+
			" MAXMODES="+ConvToStr(ServerInstance->Config->Limits.MaxModes)+
			" IDENTMAX="+ConvToStr(ServerInstance->Config->Limits.IdentMax)+
			" MAXQUIT="+ConvToStr(ServerInstance->Config->Limits.MaxQuit)+
			" MAXTOPIC="+ConvToStr(ServerInstance->Config->Limits.MaxTopic)+
			" MAXKICK="+ConvToStr(ServerInstance->Config->Limits.MaxKick)+
			" MAXGECOS="+ConvToStr(ServerInstance->Config->Limits.MaxGecos)+
			" MAXAWAY="+ConvToStr(ServerInstance->Config->Limits.MaxAway)+
			" IP6SUPPORT=1"+
			" PROTOCOL="+ConvToStr(ProtocolVersion)+extra+
			" PREFIX="+ServerInstance->Modes->BuildPrefixes()+
			" CHANMODES="+ServerInstance->Modes->GiveModeList(MODETYPE_CHANNEL)+
			" USERMODES="+ServerInstance->Modes->GiveModeList(MODETYPE_USER)+
			" SVSPART=1");

	this->WriteLine("CAPAB END");
}

/* Isolate and return the elements that are different between two comma seperated lists */
void TreeSocket::ListDifference(const std::string &one, const std::string &two, char sep,
		std::string& mleft, std::string& mright)
{
	std::set<std::string> values;
	irc::sepstream sepleft(one, sep);
	irc::sepstream sepright(two, sep);
	std::string item;
	while (sepleft.GetToken(item))
	{
		values.insert(item);
	}
	while (sepright.GetToken(item))
	{
		if (!values.erase(item))
		{
			mright.push_back(sep);
			mright.append(item);
		}
	}
	for(std::set<std::string>::iterator i = values.begin(); i != values.end(); ++i)
	{
		mleft.push_back(sep);
		mleft.append(*i);
	}
}

bool TreeSocket::Capab(const parameterlist &params)
{
	if (params.size() < 1)
	{
		this->SendError("Invalid number of parameters for CAPAB - Mismatched version");
		return false;
	}
	if (params[0] == "START")
	{
		capab->ModuleList.clear();
		capab->OptModuleList.clear();
		capab->CapKeys.clear();
		if (params.size() > 1)
			proto_version = atoi(params[1].c_str());
		SendCapabilities(2);
	}
	else if (params[0] == "END")
	{
		std::string reason;
		/* Compare ModuleList and check CapKeys */
		if ((this->capab->ModuleList != this->MyModules(VF_COMMON)) && (this->capab->ModuleList.length()))
		{
			std::string diffIneed, diffUneed;
			ListDifference(this->capab->ModuleList, this->MyModules(VF_COMMON), proto_version > 1201 ? ' ' : ',', diffIneed, diffUneed);
			if (diffIneed.length() || diffUneed.length())
			{
				reason = "Modules incorrectly matched on these servers.";
				if (diffIneed.length())
					reason += " Not loaded here:" + diffIneed;
				if (diffUneed.length())
					reason += " Not loaded there:" + diffUneed;
				this->SendError("CAPAB negotiation failed: "+reason);
				return false;
			}
		}
		if (this->capab->OptModuleList != this->MyModules(VF_OPTCOMMON) && this->capab->OptModuleList.length())
		{
			std::string diffIneed, diffUneed;
			ListDifference(this->capab->OptModuleList, this->MyModules(VF_OPTCOMMON), ' ', diffIneed, diffUneed);
			if (diffIneed.length() || diffUneed.length())
			{
				if (Utils->AllowOptCommon)
				{
					ServerInstance->SNO->WriteToSnoMask('l',
						"Optional module lists do not match, some commands may not work globally.%s%s%s%s",
						diffIneed.length() ? " Not loaded here:" : "", diffIneed.c_str(),
						diffUneed.length() ? " Not loaded there:" : "", diffUneed.c_str());
				}
				else
				{
					reason = "Optional modules incorrectly matched on these servers, and options::allowmismatch not set.";
					if (diffIneed.length())
						reason += " Not loaded here:" + diffIneed;
					if (diffUneed.length())
						reason += " Not loaded there:" + diffUneed;
					this->SendError("CAPAB negotiation failed: "+reason);
					return false;
				}
			}
		}

		if (this->capab->CapKeys.find("PROTOCOL") == this->capab->CapKeys.end())
		{
			reason = "Protocol version not specified";
		}
		else
		{
			proto_version = atoi(capab->CapKeys.find("PROTOCOL")->second.c_str());
			if (proto_version < MinCompatProtocol)
			{
				reason = "Server is using protocol version " + ConvToStr(proto_version) +
					" which is too old to link with this server (version " + ConvToStr(ProtocolVersion)
					+ (ProtocolVersion != MinCompatProtocol ? ", links with " + ConvToStr(MinCompatProtocol) + " and above)" : ")");
			}
		}

		if (capab->ChanModes.empty() && this->capab->CapKeys.find("PREFIX") != this->capab->CapKeys.end() && this->capab->CapKeys.find("PREFIX")->second != ServerInstance->Modes->BuildPrefixes())
			reason = "One or more of the prefixes on the remote server are invalid on this server.";

		if (!capab->ChanModes.empty())
		{
			if (capab->ChanModes != BuildModeList(MODETYPE_CHANNEL, proto_version))
			{
				std::string diffIneed, diffUneed;
				ListDifference(capab->ChanModes, BuildModeList(MODETYPE_CHANNEL, proto_version), ' ', diffIneed, diffUneed);
				if (diffIneed.length() || diffUneed.length())
				{
					reason = "Channel modes not matched on these servers.";
					if (diffIneed.length())
						reason += " Not loaded here:" + diffIneed;
					if (diffUneed.length())
						reason += " Not loaded there:" + diffUneed;
				}
			}
		}
		else if (this->capab->CapKeys.find("CHANMODES") != this->capab->CapKeys.end())
		{
			if (this->capab->CapKeys.find("CHANMODES")->second != ServerInstance->Modes->GiveModeList(MODETYPE_CHANNEL))
				reason = "One or more of the channel modes on the remote server are invalid on this server.";
		}

		if (!capab->UserModes.empty())
		{
			if (capab->UserModes != BuildModeList(MODETYPE_USER, proto_version))
			{
				std::string diffIneed, diffUneed;
				ListDifference(capab->UserModes, BuildModeList(MODETYPE_USER, proto_version), ' ', diffIneed, diffUneed);
				if (diffIneed.length() || diffUneed.length())
				{
					reason = "User modes not matched on these servers.";
					if (diffIneed.length())
						reason += " Not loaded here:" + diffIneed;
					if (diffUneed.length())
						reason += " Not loaded there:" + diffUneed;
				}
			}
		}
		else if (this->capab->CapKeys.find("USERMODES") != this->capab->CapKeys.end())
		{
			if (this->capab->CapKeys.find("USERMODES")->second != ServerInstance->Modes->GiveModeList(MODETYPE_USER))
				reason = "One or more of the user modes on the remote server are invalid on this server.";
		}

		/* Challenge response, store their challenge for our password */
		std::map<std::string,std::string>::iterator n = this->capab->CapKeys.find("CHALLENGE");
		if (Utils->ChallengeResponse && (n != this->capab->CapKeys.end()) && (ServerInstance->Modules->Find("m_sha256.so")))
		{
			/* Challenge-response is on now */
			this->SetTheirChallenge(n->second);
			if (!this->GetTheirChallenge().empty() && (this->LinkState == CONNECTING))
			{
				this->SendCapabilities(2);
				this->WriteLine(std::string("SERVER ")+ServerInstance->Config->ServerName+" "+this->MakePass(capab->link->SendPass, capab->theirchallenge)+" 0 "+ServerInstance->Config->GetSID()+" :"+ServerInstance->Config->ServerDesc);
			}
		}
		else
		{
			/* They didnt specify a challenge or we don't have m_sha256.so, we use plaintext */
			if (this->LinkState == CONNECTING)
			{
				this->SendCapabilities(2);
				this->WriteLine(std::string("SERVER ")+ServerInstance->Config->ServerName+" "+capab->link->SendPass+" 0 "+ServerInstance->Config->GetSID()+" :"+ServerInstance->Config->ServerDesc);
			}
		}

		if (reason.length())
		{
			this->SendError("CAPAB negotiation failed: "+reason);
			return false;
		}
	}
	else if ((params[0] == "MODULES") && (params.size() == 2))
	{
		if (!capab->ModuleList.length())
		{
			capab->ModuleList = params[1];
		}
		else
		{
			capab->ModuleList.push_back(proto_version > 1201 ? ' ' : ',');
			capab->ModuleList.append(params[1]);
		}
	}
	else if ((params[0] == "MODSUPPORT") && (params.size() == 2))
	{
		if (!capab->OptModuleList.length())
		{
			capab->OptModuleList = params[1];
		}
		else
		{
			capab->OptModuleList.push_back(' ');
			capab->OptModuleList.append(params[1]);
		}
	}
	else if ((params[0] == "CHANMODES") && (params.size() == 2))
	{
		capab->ChanModes = params[1];
	}
	else if ((params[0] == "USERMODES") && (params.size() == 2))
	{
		capab->UserModes = params[1];
	}
	else if ((params[0] == "CAPABILITIES") && (params.size() == 2))
	{
		irc::tokenstream capabs(params[1]);
		std::string item;
		while (capabs.GetToken(item))
		{
			/* Process each key/value pair */
			std::string::size_type equals = item.find('=');
			if (equals != std::string::npos)
			{
				std::string var = item.substr(0, equals);
				std::string value = item.substr(equals+1, item.length());
				capab->CapKeys[var] = value;
			}
		}
	}
	return true;
}

