/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *      the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __CMD_WHOWAS_H__
#define __CMD_WHOWAS_H__

class WhoWasMaintainer : public DataProvider
{
 public:
	WhoWasMaintainer(Module* mod) : DataProvider(mod, "whowas_maintain") {}
	virtual void AddToWhoWas(User* user) = 0;
	virtual std::string GetStats() = 0;
	virtual void PruneWhoWas(time_t t) = 0;
	virtual void MaintainWhoWas(time_t t) = 0;
};

#endif
