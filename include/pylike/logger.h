#pragma once
#ifndef PYLIKE_LOGGER2_H
#define PYLIKE_LOGGER2_H

#define LOG_ENABLE_MULTI_THREAD 1

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <codecvt>
#include <thread>
#include <unordered_map>
#include "./str.h"
#include "./os.h"
#include "./datetime.h"
#include "./argparser.h"
#include <functional>

#if LOG_ENABLE_MULTI_THREAD
#include <queue>
#include <mutex>
#include <condition_variable>
#endif

#define liststr std::vector<pystring>
#define osp os::path


#define LOG_LOC (liststr{pystring(__FILE__), std::to_string(__LINE__), pystring("<") + __func__ + ">"})


#define LOG_DEFAULT_FORMAT "$TIME | $LEVEL | $LOCATION - $MSG"

#define logadd logger.add
#define logdebug(debug_) logger.debug((debug_), LOG_LOC)
#define loginfo(info_) logger.info((info_), LOG_LOC)
#define logsuccess(success_) logger.success((success_), LOG_LOC)
#define logwarning(warning_) logger.warning((warning_), LOG_LOC)
#define logerror(error_) logger.error((error_), LOG_LOC)

#ifdef SAFE_DEFINE
#define LOG_DEBUG logger.debug(LOG_LOC)
#define LOG_INFO logger.info(LOG_LOC)
#define LOG_SUCCESS logger.success(LOG_LOC)
#define LOG_WARN logger.warning(LOG_LOC)
#define LOG_ERROR logger.error(LOG_LOC)
#define LOG_CONTINUELOG logger.info({}, false)
#else
#define DEBUG logger.debug(LOG_LOC)
#define INFO logger.info(LOG_LOC)
#define SUCCESS logger.success(LOG_LOC)
#define WARN logger.warning(LOG_LOC)
#define ERROR logger.error(LOG_LOC)
#define CONTINUELOG logger.info({}, false)
#endif

#ifdef SAFE_DEFINE
#if defined(_WIN32) || defined(_WIN64)
    #define ENDLOGALL "";logger.endAll()
    #define ENDLOG "";logger.end();
    #define LOGSHOW "";logger.end(false)
    #define LOGSHOW_IGNORE_LEVEL "";logger.end(false, true)
#else
    #define ENDLOGALL logger.endAll()
    #define ENDLOG logger.end();
    #define LOGSHOW logger.end(false)
    #define LOGSHOW_IGNORE_LEVEL logger.end(false, true)
#endif
#else
#if defined(_WIN32) || defined(_WIN64)
    #define ENDL "";logger.end()
    #define ENDLALL "";logger.endAll()
    #define ENDLOG "";logger.end();
    #define LOGSHOW "";logger.end(false)
    #define LOGSHOW_IGNORE_LEVEL "";logger.end(false, true)
#else
    #define ENDL logger.end()
    #define ENDLALL logger.endAll()
    #define ENDLALL_IGNORE_LEVEL logger.endAll(true, true)
    #define ENDLOG logger.end();
    #define LOGSHOW logger.end(false)
    #define LOGSHOW_IGNORE_LEVEL logger.end(false, true)
#endif
#endif


#define logsetMsgColored logger.setMsgColored
#define logsetStdoutLevel logger.setStdoutLevel
#define logsetStdoutFormat logger.setStdoutFormat
#define logsetStdoutTimeFormat logger.setStdoutTimeFormat
#define logsetLevelColor logger.setLevelColor
#define logsetLocationColor logger.setLocationColor
#define logsetTimeColor logger.setTimeColor
#define logsetStdoutFuncNameShow logger.setStdoutFuncNameShow
#define logsetShowLocation logger.setLocationShow

#define logsetFromParser logger.setFromParser
#define logadd2Parser logger.addLogParser
#define logaddAndSetFromParser logger.addAndSetFromParser

#define logsetFromParser2 logger.setFromParser2
#define logadd2Parser2 logger.addLogParser2
#define logaddAndSetFromParser2 logger.addAndSetFromParser2


enum LogLevel
{
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_SUCCESS,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR
};


enum LogSettings {
    LOG_COLOR_BLUE,
    LOG_COLOR_GREEN,
    LOG_COLOR_YELLOW,
    LOG_COLOR_RED,
    LOG_COLOR_PURPLE,
    LOG_COLOR_CYAN,
    LOG_COLOR_WHITE,
    LOG_FONT_BOLD,
    LOG_NORMAL,
};


enum PathType {
    LOG_PATH_ABS,
    LOG_PATH_REL,
    LOG_PATH_BASE
};


struct SingleLog
{
    enum Content {
        Time,
        Level,
        Location,
        Msg,
        Other
    };

    struct PartInfo {
        Content content;
        pystring text;
    };
    bool isStdout = false;
    bool showFuncName = true;
    pystring filePath;
    LogLevel level=LOG_LEVEL_INFO;
    pystring format;
    pystring timeformat = "%Y-%m-%d %H:%M:%S.%ms";
    std::vector<PartInfo> infos;
    // std::ofstream* outfile=nullptr;

    PathType pathtype = LOG_PATH_BASE;


    // void openFile()
    // {
    //     if (osp::isfile(filePath) && outfile == nullptr)
    //     {
    //         outfile = new std::ofstream(filePath, std::ios::app);
    //     }
    // }

    // void closeFile()
    // {
    //     if (outfile == nullptr) return;
    //     if(outfile->is_open())
    //     {
    //         outfile->close();
    //         delete outfile;
    //         outfile = nullptr;
    //     }
    // }
    

    std::vector<PartInfo> _decodeFormat(std::vector<PartInfo> texts, pystring kw, Content ctype) {
        std::vector<PartInfo> ret;
        for (PartInfo info: texts) {
            if (info.content == Other) {
                
                std::vector<pystring> splits = info.text.split(kw);
                if (splits.size() < 2) {
                    ret.push_back(info);
                    continue;
                }

                for (int i=0;i<splits.size();i++) {
                    if (i) {
                        PartInfo kwinfo;
                        kwinfo.content = ctype;
                        ret.push_back(kwinfo);
                    }
                    PartInfo newinfo;
                    newinfo.content = Other;
                    newinfo.text = splits[i];
                    ret.push_back(newinfo);
                }
            }
            else {
                ret.push_back(info);
            }
                
        }
        return ret;
    }

    void decodeFormat() {
        if (!format.length() || format.empty()) format = LOG_DEFAULT_FORMAT;
        infos.clear();
        int count = 0;
        PartInfo info_;
        info_.content = Other;
        info_.text = format;
        infos = {info_};
        for (pystring kw: {"$TIME", "$LEVEL", "$LOCATION", "$MSG"}) {
            infos = _decodeFormat(infos, kw, (Content)count);
            count++;
        }
    }

};



class Logger {
public:

    Logger() {
        stdoutLog.level = LOG_LEVEL_DEBUG;
        stdoutLog.format = LOG_DEFAULT_FORMAT;
        stdoutLog.isStdout = true;
        stdoutLog.decodeFormat();
        LogSetFile.enabled = false;
        LogSetStdOut.enabled = true;

#if LOG_ENABLE_MULTI_THREAD
        if (!threadOpen)
        {
            writeFileThread = std::thread(&Logger::writeFileService, this);
            threadOpen = true;
            writeFileThread.detach();
        }
#endif
        
    }

    ~Logger()
    {
        closeAllFileLog();
        end_all = true;
        conditionVar.notify_all();
        if (writeFileThread.joinable())
        {
            writeFileThread.join();
        }
    }

    void showStdout(bool enabled=true) {
        show_ = enabled;
    }

    void setStdoutFuncNameShow(bool flag=true) {
        stdoutLog.showFuncName = flag;
    }

    void setStdoutTimeFormat(pystring format) {
        stdoutLog.timeformat = format;
    }

    void setStdoutLevel(LogLevel level) {
        stdoutLog.level = level;
    }

    void setStdoutLevel(int level) {
        level = MIN((int)LOG_LEVEL_ERROR, level);
        stdoutLog.level = (LogLevel)level;
    }

    void setAllFilesLevel(int level)
    {
        level = MIN((int)LOG_LEVEL_ERROR, level);
        for(int i=0;i<logs.size();i++)
        {
            logs[i].level = (LogLevel)level;
        }
        maxFileLevel = (LogLevel)level;
    }

    void setAllFilesFormat(pystring format)
    {
        for(int i=0;i<logs.size();i++)
        {
            logs[i].format = format;
        }
    }

    void setStdoutFormat(pystring format) {
        stdoutLog.format = format;
        stdoutLog.decodeFormat();
    }

    void setLevelColor(LogLevel level_, LogSettings color_) {
        levelColor_.at(level_) = color_;
    }

    void setTimeColor(LogSettings color_) {
        timeColor_ = color_;
    }

    void setLocationColor(LogSettings color_) {
        locationColor_ = color_;
    }

    void setMsgColored(bool flag) {
        msg_color = flag;
    }

    void setLocationShow(bool flag) {
        show_location = flag;
    }

    bool add(pystring filepath, LogLevel level=LOG_LEVEL_INFO, pystring format="", pystring timeformat="", bool generate_path=true) {
        if (!filepath.length()) return false;
        pystring dirname = osp::dirname(osp::abspath(filepath));
        
        // info(LOG_LOC) << "log saved dir is '" << dirname << "'" << end();
        if (!osp::isdir(dirname)) {
            if (generate_path) 
            {
                // info(LOG_LOC) << "create dir '" << dirname << "'" << end();
                os::makedirs(dirname);
                if (!osp::isdir(dirname))
                {
                    error(LOG_LOC) << "failed to create dir '" << dirname << "'" << end();
                }
            }
            else {
                error(LOG_LOC) << "path '" << dirname 
                               << "' not exist! Skip adding log file '" 
                               << filepath << "'." << end();
                return false;
            }
        }
        
        SingleLog log2add;
        log2add.filePath = filepath;
        log2add.level = level;
        log2add.format = format;
        log2add.timeformat = timeformat;
        log2add.decodeFormat();
        logs.push_back(log2add);
        maxFileLevel = ((int)level>(int)maxFileLevel)?level:maxFileLevel;
        // log2add.openFile();
        // if (log2add.outfile != nullptr)
        // {
        //     if (!log2add.outfile->is_open())
        //     {
        //         error(LOG_LOC) << "failed to open " << filepath << end();
        //     }
        //     else
        //     {
        //         logs.push_back(log2add);
        //     }
        // }
        
        return true;
    }

    void remove(pystring filepath) {
        if (filepath.length()) {
            std::vector<SingleLog> logs_;
            for (int i=0;i<logs.size();i++) {
                if (!(logs[i].filePath == filepath)) {
                    logs_.push_back(logs[i]);
                }
            }
            logs = logs_;
        }
    }

    void log(LogLevel level, pystring msg, liststr location={}) {
        location_ = location;
        this->_log(location, level, msg);
    }

    void debug(pystring debug_, liststr location={}) {
        log(LOG_LEVEL_DEBUG, debug_, location);
    }

    void info(pystring info_, liststr location={}) {
        log(LOG_LEVEL_INFO, info_, location);
    }

    void success(pystring info_, liststr location={}) {
        log(LOG_LEVEL_SUCCESS, info_, location);
    }

    void warning(pystring warning_, liststr location={}) {
        log(LOG_LEVEL_WARNING, warning_, location);
    }

    void error(pystring error_, liststr location={}) {
        log(LOG_LEVEL_ERROR, error_, location);
    }

    enum class LogLineEndSignal
    {
        LOG_LINE_END_NORMAL=0,
        LOG_LINE_END_FORCE_SHOW=1,                  // 0001, force show
        LOG_LINE_END_ENDALL=2,                      // 0010, end all
        LOG_LINE_END_ENDALL_AND_FORCE_SHOW=3,       // 0011, end all, force_show
        LOG_LINE_END_WRITE_FILE=4,                  // 0100,  write file
        LOG_LINE_END_WRITE_FILE_AND_FORCE_SHOW=5,   // 0101, write file, force_show
        LOG_LINE_END_WRITE_FILE_AND_ENDALL=6,       // 0110, write file, end all
        LOG_LINE_END_WRITE_FILE_AND_ENDALL_AND_FORCE_SHOW=7 // 0111
    };

    LogLineEndSignal end(bool write2file=true, bool force_show=false) {
        end_all = false;
        int flag = force_show?1:0 + write2file?4:0;
        return (LogLineEndSignal)flag;
    }

    LogLineEndSignal endAll(bool write2file=true, bool force_show=false) {
        end_all = true;
        int flag = 2 + force_show?1:0 + write2file?4:0;
        return (LogLineEndSignal)flag;
    }

    class OneLogLine
    {
    public:
        struct LineData
        {
            pystring msg;
            std::vector<pystring> locations;
            LogLevel level;
            datetime::Datetime date;
            bool force_show=false;
        };

        OneLogLine(Logger* logger, std::vector<pystring> locations, LogLevel level)
        {
            logger_ = logger;
            data.locations = locations;
            data.level = level;
            isUsing_ = true;
            data.date = datetime::datetime::now();
        }

        void operator<<(LogLineEndSignal sig)
        {
            // logger_->log(level_, oss.str(), locations_);
            data.msg = oss.str();
            logger_->_log2(data, (int)sig);
            isUsing_ = false;
            delete this;
        }

        template <class T>
        OneLogLine& operator<<(T value)
        {
            oss << value;
            return *this;
        }

        bool isUsing_=false;
    private:
        std::ostringstream oss;
        LineData data;
        Logger* logger_ = nullptr;
    };

    OneLogLine& log(LogLevel level, liststr location={}, bool clear=true)
    {
        auto line = new OneLogLine(this, location, level);
        return *line;
    }

    OneLogLine& debug(liststr location={}, bool clear=true) {
        return log(LOG_LEVEL_DEBUG, location, clear);
    }

    OneLogLine& info(liststr location={}, bool clear=true) {
        return log(LOG_LEVEL_INFO, location, clear);
    }

    OneLogLine& success(liststr location={}, bool clear=true) {
        return log(LOG_LEVEL_SUCCESS, location, clear);
    }

    OneLogLine& warning(liststr location={}, bool clear=true) {
        return log(LOG_LEVEL_WARNING, location, clear);
    }

    OneLogLine& error(liststr location={}, bool clear=true) {
        return log(LOG_LEVEL_ERROR, location, clear);
    }

// protected:
    SingleLog* getLogByName(pystring filepath) {
        if (filepath.length()) {
            std::vector<SingleLog> logs_;
            for (int i=0;i<logs.size();i++) {
                if (logs[i].filePath == filepath) {
                    return &logs[i];
                }
            }
            logs = logs_;
        }
        return nullptr;
    }

    void addLogParser(argparser::ArgumentParser& parser, int defaultLogLevel=0, int defaultShowLevel=0)
    {
        parser.add_option<std::string>("", "--add-log", "log file name", "");
        parser.add_option<int>("", "--log-level", "log level (0: debug, 1: info, 2: success, 3: warning, 4: error)", std::move(defaultLogLevel));
        parser.add_option<int>("", "--show-level", "show log level, same as log level", std::move(defaultShowLevel));
    }

    void setFromParser(argparser::ArgumentParser& parser)
    {
        if (!parser.isOptionDefined("--add-log")||!parser.isOptionDefined("--log-level")||!parser.isOptionDefined("--show-level"))
        {
            error("no required option, please check" ,LOG_LOC);
            return;
        }

        std::string logfile = parser.get_option_string("--add-log");
        int level = parser.get_option_int("--log-level");
        setStdoutLevel(parser.get_option_int("--show-level"));
        if (add(logfile, (LogLevel)level)) 
        {
            auto filelogger = getLogByName(logfile);
            filelogger->pathtype = LOG_PATH_ABS;
        }
    }

    void addAndSetFromParser(argparser::ArgumentParser& parser, int defaultLogLevel=0, int defaultShowLevel=0)
    {
        addLogParser(parser, defaultLogLevel, defaultShowLevel);
        parser.parse();
        std::string logfile = parser.get_option_string("--add-log");
        int level = parser.get_option_int("--log-level");
        setStdoutLevel(parser.get_option_int("--show-level"));
        if (add(logfile, (LogLevel)level)) 
        {
            auto filelogger = getLogByName(logfile);
            filelogger->pathtype = LOG_PATH_ABS;
        }
    }

    template <class Parser>
    void addLogParser2(Parser& parser, int defaultLogLevel=0, int defaultShowLevel=0)
    {
        parser.add_argument({"--add-log"}, "", "log file name");
        parser.add_argument({"--log-level"}, std::move(defaultLogLevel), "log level (0: debug, 1: info, 2: success, 3: warning, 4: error)");
        parser.add_argument({"--show-level"}, std::move(defaultShowLevel), "show log level, same as log level");
    }

    template <class Parser>
    void setFromParser2(Parser& parser)
    {
        if (!parser["add-log"].isValid()||!parser["log-level"].isValid()||!parser["show-level"].isValid())
        {
            error("no required option, please check" ,LOG_LOC);
            return;
        }

        std::string logfile = parser["add-log"];
        int level = parser["log-level"];
        setStdoutLevel((int)parser["show-level"]);
        if (add(logfile, (LogLevel)level)) 
        {
            auto filelogger = getLogByName(logfile);
            filelogger->pathtype = LOG_PATH_ABS;
        }
    }

    template <class Parser>
    void addAndSetFromParser2(Parser& parser, int defaultLogLevel=0, int defaultShowLevel=0)
    {
        addLogParser2(parser, defaultLogLevel, defaultShowLevel);
        parser.parse_args();
        std::string logfile = parser["add-log"];
        int level = parser["log-level"];
        setStdoutLevel((int)parser["show-level"]);
        if (add(logfile, (LogLevel)level)) 
        {
            auto filelogger = getLogByName(logfile);
            filelogger->pathtype = LOG_PATH_ABS;
        }
    }

private:
    bool show_location=true;
    bool end_all = false;
    std::vector<SingleLog> logs;
    SingleLog stdoutLog;
    LogLevel maxFileLevel=LOG_LEVEL_DEBUG;
    std::thread writeFileThread;

#if LOG_ENABLE_MULTI_THREAD
    std::mutex queue_mutex;
    std::queue<OneLogLine::LineData> linedatas;
    std::condition_variable conditionVar;
#endif

    std::unordered_map<LogLevel, LogSettings> levelColor_ = {
        {LOG_LEVEL_DEBUG, LOG_COLOR_BLUE},
        {LOG_LEVEL_INFO, LOG_FONT_BOLD},
        {LOG_LEVEL_SUCCESS, LOG_COLOR_GREEN},
        {LOG_LEVEL_WARNING, LOG_COLOR_YELLOW},
        {LOG_LEVEL_ERROR, LOG_COLOR_RED}
    };

    std::unordered_map<LogLevel, pystring> levelFlag_ = {
        {LOG_LEVEL_DEBUG, "DEBUG   "},
        {LOG_LEVEL_INFO, "INFO    "},
        {LOG_LEVEL_SUCCESS, "SUCCESS "},
        {LOG_LEVEL_WARNING, "WARNING "},
        {LOG_LEVEL_ERROR, "ERROR   "}
    };

    LogSettings timeColor_ = LOG_COLOR_GREEN;
    LogSettings locationColor_ = LOG_COLOR_CYAN;

    bool msg_color = true;

    liststr location_;

    std::ostringstream oss;
    LogLevel oss_level = LOG_LEVEL_INFO;

    bool show_ = true;

    struct _LogSet
    {
        std::unordered_map<LogSettings, pystring> logset = {
            {LOG_COLOR_BLUE, "\033[34m"},
            {LOG_COLOR_GREEN, "\033[32m"},
            {LOG_COLOR_YELLOW, "\033[33m"},
            {LOG_COLOR_RED, "\033[31m"},
            {LOG_COLOR_PURPLE, "\033[35m"},
            {LOG_COLOR_CYAN, "\033[36m"},
            {LOG_COLOR_WHITE, "\033[37m"},
            {LOG_FONT_BOLD, "\033[1m"},
            {LOG_NORMAL, "\033[0m"}
        };

        bool enabled = true;

        pystring setText(pystring text, pystring logset_) {
            if (enabled) return logset_ + text + logset.at(LOG_NORMAL);
            else return text;
        }

        pystring setText(pystring text, LogSettings set_) {
            if (enabled) return setText(text, logset.at(set_));
            else return text;
        }

    };

    _LogSet LogSetFile, LogSetStdOut;

    pystring timestr(pystring format_, _LogSet LogSet) {
        pystring ret = "";
        auto date = datetime::datetime::now().strftime(format_);
        ret = LogSet.setText(date, timeColor_);
        return ret;
    }

    pystring timestr(pystring format_, datetime::Datetime date_, _LogSet LogSet) {
        pystring ret = "";
        ret = LogSet.setText(date_.strftime(format_), timeColor_);
        return ret;
    }

    pystring getColorByLevel(LogLevel level, _LogSet LogSet) {
        return LogSet.logset.at(levelColor_.at(level));
    }

    pystring levelstr(LogLevel level, _LogSet LogSet) {
        return LogSet.setText(LogSet.setText(levelFlag_.at(level), levelColor_.at(level)), LOG_FONT_BOLD);
    }

    inline pystring writeOneLogString(SingleLog& log, LogLevel level, pystring msg, datetime::Datetime date, std::vector<pystring> location, bool force_show=false)
    {
        pystring logstr = "";

        _LogSet LogSet = log.isStdout?LogSetStdOut:LogSetFile;

        for (auto c: log.infos) {
            if (c.content == SingleLog::Content::Other) {
                logstr += c.text;
            }
            else if (c.content == SingleLog::Content::Time)
            {
                logstr += timestr(log.timeformat, date, LogSet);
            }
            else if (c.content == SingleLog::Content::Location)
            {
                if (!show_location) continue;
                pystring info = "";
                for (int i=0;i<location.size();i++) {
                    if(!i) {
                        if (log.pathtype == LOG_PATH_BASE) location[i] = osp::basename(location[i]);
                        else if (log.pathtype == LOG_PATH_REL) location[i] = osp::relpath(location[i]);
                        else location[i] = osp::abspath(location[i]);
                    }
                    else if(!log.showFuncName && i == location.size()-1) continue;
                    if (i) info += ":";
                    info += LogSet.setText(location[i], locationColor_);
                }
                logstr += info; //LogSet.setText(info, locationColor_);
            }
            else if (c.content == SingleLog::Content::Level)
            {
                logstr += levelstr(level, LogSet);
            }
            else if (c.content == SingleLog::Content::Msg)
            {
                auto text = LogSet.setText(msg, LOG_FONT_BOLD);
                if (msg_color) text = LogSet.setText(text, levelColor_.at(level));
                logstr += text;
            }
        }
        return logstr;
    }

    void writeOneLog(SingleLog& log, LogLevel level, pystring msg, datetime::Datetime date, std::vector<pystring> location, bool force_show=false) 
    {
        if (level < log.level && !force_show) return;
        if (log.isStdout && !show_) return;

        pystring logstr = writeOneLogString(log, level, msg, date, location, force_show);

        if (log.isStdout) {
            // printf(logstr.c_str());
            logstr += "\n";
            printf("%s", logstr.c_str());
        } else {
            // writefile(log, logstr);
            writefile(logstr, log.filePath);
        }
    }

    /**
     * @brief Converts a string to UTF-8 encoding.
     * 
     * Takes an input string and converts it to UTF-8 encoding using wide character conversion.
     * The conversion is done via std::wstring_convert with std::codecvt_utf8.
     * 
     * @param input The input string to be converted.
     * @return std::string The UTF-8 encoded string.
     */
    inline std::string toUTF8(const std::string& input)
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8converter;
        // std::cout << input << std::endl;
        std::wstring wideString = utf8converter.from_bytes(input.c_str());
        return utf8converter.to_bytes(wideString);
        
    }

    // inline void writefile(SingleLog& log, pystring msg)
    // {
    //     msg += "\n";
    //     // *log.outfile << toUTF8(msg);
    //     std::string str2write = toUTF8(msg);
    //     log.outfile->write(str2write.c_str(), str2write.size());
    // }

    void writefile(pystring msg, pystring filepath) 
    {
        std::ofstream outfile(filepath.c_str(), std::ios::app);  // 创建ofstream对象，并打开文件
        if (!outfile.is_open()) { // 检查文件是否成功打开
            error(LOG_LOC) << "failed to open " << filepath << end();
            return;
        }
        msg += "\n";
        outfile << toUTF8(msg); // 写入文件
        outfile.close(); // 关闭文件

        // pystring command = pystring("echo '") + msg + "' >> " + filepath;
        // popen(command.c_str(), "r");
        // // system(command.c_str());
    }
    
    inline void closeAllFileLog()
    {
        // for (SingleLog& log: logs) log.closeFile();
        logs.clear();
    }

    inline void __logfile(LogLevel level, pystring msg, datetime::Datetime d, std::vector<pystring> loc) {
        for (SingleLog& log: logs) {
            // std::cout << "write log" << std::endl;
            writeOneLog(log, level, msg, d, loc);
        }
        if (end_all)
        {
            closeAllFileLog();
        }
    }

#if LOG_ENABLE_MULTI_THREAD
    void writeFileService()
    {
        // std::ofstream(filePath, std::ios::app);
        std::vector<std::ofstream> fouts(logs.size());
        for (int i=0;i<logs.size();i++)
        {
            fouts[i] = std::ofstream(logs[i].filePath, std::ios::app);
        }
        while (true)
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            conditionVar.wait(lock, [this](){
                return this->linedatas.size() || this->end_all;
            });
            
            if (!this->linedatas.size())
            {
                for (int i=0;i<fouts.size();i++) fouts[i].close();
                return;
            }

            if (logs.size() > fouts.size())
            {
                size_t ori_size = fouts.size();
                fouts.resize(logs.size());
                for (int i=ori_size;i<logs.size();i++)
                {
                    fouts[i] = std::ofstream(logs[i].filePath, std::ios::app);
                }
            }

            while(linedatas.size())
            {
                auto data = linedatas.front();
                linedatas.pop();
                // std::cout << data.msg << std::endl;
                // __logfile(data.level, data.msg, data.date, data.locations);
                // if (data.level >= maxFileLevel)
                // {
                    for (int i=0;i<logs.size();i++) {
                        if(logs[i].level > data.level) continue;
                        pystring line2write = writeOneLogString(logs[i], data.level, data.msg, data.date, data.locations, data.force_show);
                        line2write += "\n";
                        fouts[i] << toUTF8(line2write);
                        // std::cout << "write log: " <<  fouts[i].is_open() << std::endl;
                    }
                // }
            }
        }
    }

    bool threadOpen=false;
    std::mutex push_mutex;
    std::condition_variable pushCV;
    bool pushing=false;
#endif

    void _log2(OneLogLine::LineData data, int flag=0) 
    {
        bool write2file=(flag >= 4) && ((int)data.level >= (int)maxFileLevel);
        data.force_show=((flag % 2) == 1);
        // std::thread _t;
        

        if(write2file)
        {
#if LOG_ENABLE_MULTI_THREAD
            std::unique_lock<std::mutex> lock(push_mutex);
            pushCV.wait(lock, [this](){
                return !this->pushing;
            });
            // while (pushing) pytime::sleep(0.0001);
            pushing = true;
            linedatas.push(data);
            pushing = false;
            lock.unlock();
            conditionVar.notify_one();
            pushCV.notify_one();
#else
            for (SingleLog& log: logs) 
            {
                writeOneLog(log, data.level, data.msg, data.date, data.locations);
            }
#endif
        }
        // else
        // {
        //     std::cout << flag << ", " << (int)data.level << ", " << (int)maxFileLevel << std::endl;
        // }

        writeOneLog(stdoutLog, data.level, data.msg, data.date, data.locations, data.force_show);
    }

    void _log(std::vector<pystring> loc, LogLevel level, pystring msg, bool write2file=true, bool force_show=false) {
        datetime::Datetime d = datetime::datetime::now();
        std::thread _t;
        if(write2file)
        {
            // std::cout << "write log" << std::endl;
            // _t = std::thread(std::mem_fn(&Logger::__logfile), this, level, msg, d, loc);
            OneLogLine::LineData data;
            data.date = d;
            data.level = level;
            data.msg = msg;
            data.locations = loc;
            data.force_show = force_show;
            // std::unique_lock<std::mutex> lock(queue_mutex);
#if LOG_ENABLE_MULTI_THREAD
            std::unique_lock<std::mutex> lock(push_mutex);
            pushCV.wait(lock, [this](){
                return !this->pushing;
            });
            // while (pushing) pytime::sleep(0.0001);
            pushing = true;
            linedatas.push(data);
            pushing = false;
            conditionVar.notify_one();
            // lock.unlock();
            pushCV.notify_one();
#else
            for (SingleLog& log: logs) 
            {
                writeOneLog(log, data.level, data.msg, data.date, data.locations);
            }
#endif
            // std::cout << linedatas.size() << std::endl;
            
            // _t.join();
            // std::cout << "write log end" << std::endl;
            
        }
        writeOneLog(stdoutLog, level, msg, d, loc, force_show);
    }

};

static Logger logger;


#endif
