/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifdef CONF_DISCORD
#include "discord_main.h"
#include "discord_slash_commands.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include <mutex>

std::mutex ml_mutex_task;
DiscordJob::DiscordJob(IServer* pServer) : SleepyDiscord::DiscordClient(g_Config.m_SvDiscordToken, SleepyDiscord::USER_CONTROLED_THREADS)
{
	m_pServer = pServer;
	setIntents(SleepyDiscord::Intent::SERVER_MESSAGES);
	
	// start thread discord event bot
	std::thread(&DiscordJob::run, this).detach();
	// start handler bridge teeworlds - discord bot
	std::thread(&DiscordJob::HandlerThreadTasks, this).detach(); 
}

void DiscordJob::onReady(SleepyDiscord::Ready readyData)
{
	DiscordCommands::InitCommands(this);
}

void DiscordJob::onInteraction(SleepyDiscord::Interaction interaction)
{
	const auto pBtnItem = std::find_if(m_aBtnCallbacks.begin(), m_aBtnCallbacks.end(), [&interaction](const StructButtonCallback& pItem) { return str_comp(interaction.data.customID.c_str(), pItem.m_aButtonID) == 0; });
	if(pBtnItem != m_aBtnCallbacks.end() && pBtnItem->m_BtnCallback)
	{
		pBtnItem->m_BtnCallback(this, &interaction);
		return;
	}
	
	DiscordCommands::ExecuteCommand(this, &interaction);
}

/************************************************************************/
/* Discord main events functions                                        */
/************************************************************************/
void DiscordJob::onAddMember(SleepyDiscord::Snowflake<SleepyDiscord::Server> serverID, SleepyDiscord::ServerMember member)
{
	// welcome messages
	std::vector < std::string > ArrayWelcomes;
	ArrayWelcomes.push_back("I can't believe my eyes! Wipe my window " + member.user.showMention());
	ArrayWelcomes.push_back(member.user.showMention() + ", i bet you haven't seen me as long as i had!");
	ArrayWelcomes.push_back("Welcome, " + member.user.showMention() + ". We hope you're not coming to us without pizza!");
	ArrayWelcomes.push_back("The raptor "+ member.user.showMention() + " appeared. Watch out!");
	std::string RulesStr("\n**Don't forget to read the rules <#708092196024352768>!**");

	const int RandomID = random_int() % ArrayWelcomes.size();
	std::string Fullmessage(ArrayWelcomes[RandomID] + RulesStr);

	SleepyDiscord::Embed EmbedWelcome;
	EmbedWelcome.color = DC_INVISIBLE_GRAY;
	EmbedWelcome.description = Fullmessage;
	sendMessage(g_Config.m_SvDiscordWelcomeChannel, "\0", EmbedWelcome);

	// give member role
	addRole(serverID, member.ID, g_Config.m_SvDiscordWelcomeRole);
}

void DiscordJob::onMessage(SleepyDiscord::Message message)
{
	// skip empty and messages from the bot itself
	if(message.length() <= 0 || message.author == getCurrentUser().cast())
	 	return;

	// send from the discord chat to server chat
	if(str_comp(std::string(message.channelID).c_str(), g_Config.m_SvDiscordServerChatChannel) == 0)
	{
		std::string Nickname("D|" + message.author.username);
		m_pServer->GameServer(FAKE_DISCORD_WORLD_ID)->FakeChat(Nickname.c_str(), message.content.c_str());
		return;
	}

	// suggestions-voting
	if(str_comp(std::string(message.channelID).c_str(), g_Config.m_SvDiscordSuggestionChannel) == 0)
	{
		deleteMessage(message.channelID, message);

		SleepyDiscord::Embed EmbedIdeas;
		EmbedIdeas.title = std::string("Suggestion");
		EmbedIdeas.description = "From: " + message.author.showMention() + "\n" + message.content;
		EmbedIdeas.color = DC_DISCORD_INFO;
		EmbedIdeas.thumbnail.url = message.author.avatarUrl();
		EmbedIdeas.thumbnail.proxyUrl = message.author.avatarUrl();
		EmbedIdeas.footer.text = "Use reactions for voting!";
		EmbedIdeas.footer.iconUrl = message.author.avatarUrl();
		EmbedIdeas.footer.proxyIconUrl = message.author.avatarUrl();

		SleepyDiscord::Message pMessage = sendMessage(message.channelID, "\0", EmbedIdeas);
		addReaction(message.channelID, pMessage, "%E2%9C%85");
		addReaction(message.channelID, pMessage, "%E2%9D%8C");
	}
}

/************************************************************************/
/* Discord main functions                                               */
/************************************************************************/
void DiscordJob::SendSuccesfulMessage(SleepyDiscord::Snowflake<SleepyDiscord::Channel> channelID, std::string Message)
{
	SleepyDiscord::Embed EmbedSuccessful;
	EmbedSuccessful.description = Message;
	EmbedSuccessful.color = DC_DISCORD_SUCCESS;
	sendMessage(channelID, "\0", EmbedSuccessful);
}

void DiscordJob::SendWarningMessage(SleepyDiscord::Snowflake<SleepyDiscord::Channel> channelID, std::string Message)
{
	SleepyDiscord::Embed EmbedWarning;
	EmbedWarning.description = Message;
	EmbedWarning.color = DC_DISCORD_WARNING;
	sendMessage(channelID, "\0", EmbedWarning);
}

bool DiscordJob::SendGenerateMessage(SleepyDiscord::User UserRequestFrom, std::string Channel, std::string Title, std::string SearchNickname, int Color, bool MultipleSearch)
{
	CSqlString<64> SearchNick(std::string("%" + SearchNickname + "%").c_str());
	ResultPtr pRes = SJK.SD("*", "tw_accounts_data", "WHERE Nick LIKE '%s' LIMIT %d", SearchNick.cstr(), (MultipleSearch ? 3 : 1));
	while(pRes->next())
	{
		const int AccountID = pRes->getInt("ID");
		const int Rank = Server()->GameServer()->GetRank(AccountID);
		std::string Nickname(pRes->getString("Nick").c_str());
		std::string PhpArguments = "?player=" + Nickname + "&dicid=" + std::to_string(pRes->getInt("DiscordEquip")) + "&rank=" + std::to_string(Rank);
		std::string ImageUrl = std::string(g_Config.m_SvDiscordGenerateURL) + PhpArguments;

		SleepyDiscord::Embed embed;
		embed.title = Title;
		embed.image.height = 800;
		embed.image.width = 600;
		embed.image.url = ImageUrl;
		embed.image.proxyUrl = ImageUrl;
		embed.color = Color != 0 ? Color : 16353031;
		std::string DiscordID(pRes->getString("DiscordID").c_str());
		if(DiscordID.compare("null") != 0)
		{
			SleepyDiscord::User UserAccountOwner = getUser(DiscordID).cast();
			if(!UserAccountOwner.invalid())
			{
				embed.description = "Account owner: " + UserAccountOwner.showMention();
				embed.thumbnail.url = UserAccountOwner.avatarUrl(48);
				embed.thumbnail.proxyUrl = UserAccountOwner.avatarUrl(48);
				embed.thumbnail.height = 48;
				embed.thumbnail.width = 48;
			}
		}
		if(!UserRequestFrom.invalid())
		{
			embed.footer.text = "Request from " + UserRequestFrom.username;
			embed.footer.iconUrl = UserRequestFrom.avatarUrl();
			embed.footer.proxyIconUrl = UserRequestFrom.avatarUrl();
		}

		this->sendMessage(Channel, "\0", embed);
	}
	const bool Founded = (pRes->rowsCount() > 0);
	return Founded;
}

bool DiscordJob::SendGenerateMessageAccountID(SleepyDiscord::User UserRequestFrom, std::string Chanal, std::string Title, int AccountID, int Color)
{
	CGS* pGS = (CGS*)Server()->GameServer(MAIN_WORLD_ID);
	std::string Nickname(pGS->Mmo()->PlayerName(AccountID));
	return SendGenerateMessage(UserRequestFrom, Chanal, Title, Nickname.c_str(), Color, false);
}

// TODO: Rework it need impl it for easy use and safe
void DiscordJob::CreateButton(std::shared_ptr<SleepyDiscord::ActionRow> pActionRow, const char* pInteractive, const char* pLabel, SleepyDiscord::ButtonStyle ButtonStyle, bool Disabled, ButtonCallback pBtnCallback)
{
	auto pBtn = std::make_shared<SleepyDiscord::Button>();
	pBtn->style = ButtonStyle;
	pBtn->label = pLabel;
	if(ButtonStyle == SleepyDiscord::ButtonStyle::Link)
		pBtn->url = pInteractive;
	else
		pBtn->customID = pInteractive;
	
	if(pBtnCallback)
	{
		const auto pItem = std::find_if(m_aBtnCallbacks.begin(), m_aBtnCallbacks.end(), [&pInteractive](const StructButtonCallback& pItem)
		{ return str_comp(pItem.m_aButtonID, pInteractive) == 0; });
		if(pItem == m_aBtnCallbacks.end())
		{
			StructButtonCallback ButtonCallback;
			str_copy(ButtonCallback.m_aButtonID, pInteractive, sizeof(ButtonCallback.m_aButtonID));
			ButtonCallback.m_BtnCallback = pBtnCallback;
			m_aBtnCallbacks.push_back(ButtonCallback);
		}
		else
			pItem->m_BtnCallback = pBtnCallback;
	}
	
	pBtn->disabled = Disabled;
	pActionRow->components.push_back(pBtn);
}

/************************************************************************/
/* Discord teeworlds server side                                        */
/************************************************************************/
void DiscordJob::HandlerThreadTasks()
{
	while(true)
	{
		ml_mutex_task.lock();
		if(m_pThreadHandler.empty())
		{
			ml_mutex_task.unlock();
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}

		for(DiscordHandle* pHandler : m_pThreadHandler)
		{
			pHandler->m_pEvent();
			delete pHandler;
		}
		m_pThreadHandler.clear();
		ml_mutex_task.unlock();
	}
}

void DiscordJob::AddThreadTask(DiscordTask Task)
{
	ml_mutex_task.lock();
	m_pThreadHandler.push_back(new DiscordHandle(Task));
	ml_mutex_task.unlock();
}

#endif