#ifndef _SPANNINGTREE_PROTOCOL_INT_
#define _SPANNINGTREE_PROTOCOL_INT_

class SpanningTreeUtilities;
class ModuleSpanningTree;


class SpanningTreeProtocolInterface : public ProtocolInterface
{
	SpanningTreeUtilities* Utils;
	ModuleSpanningTree* Module;
 public:
        SpanningTreeProtocolInterface(ModuleSpanningTree* mod, SpanningTreeUtilities* util, InspIRCd* Instance) : ProtocolInterface(Instance), Utils(util), Module(mod) { }
        virtual ~SpanningTreeProtocolInterface() { }

        virtual void SendEncapsulatedData(parameterlist &encap);
        virtual void SendMetaData(void* target, int type, const std::string &key, const std::string &data);
        virtual void SendTopic(Channel* channel, std::string &topic);
        virtual void SendMode(const std::string &target, parameterlist &modedata);
        virtual void SendOperNotice(const std::string &text);
        virtual void SendModeNotice(const std::string &modes, const std::string &text);
        virtual void SendSNONotice(const std::string &snomask, const std::string &text);
        virtual void PushToClient(User* target, const std::string &rawline);
};

#endif