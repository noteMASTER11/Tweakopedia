#include "app/EncyclopediaReferenceArticles.h"

#include <algorithm>

using namespace Qt::StringLiterals;

namespace tweakopedia::app {

const QVector<EncyclopediaReferenceArticle>& encyclopediaReferenceArticles()
{
    static const QVector<EncyclopediaReferenceArticle> articles{
        {
            .id = u"guide.system-integrity"_s,
            .title = u"Проверка и восстановление хранилища компонентов Windows"_s,
            .summary = u"Как читать результат DISM, восстанавливать хранилище компонентов и действовать при ошибках восстановления."_s,
            .purpose = u"Хранилище компонентов содержит эталонные данные, из которых Windows обслуживает системные компоненты и восстанавливает защищённые файлы."_s,
            .categoryId = u"diagnostic-guides"_s,
            .categoryTitle = u"Диагностика и восстановление"_s,
            .subcategoryId = u"system-integrity"_s,
            .subcategoryTitle = u"Целостность Windows"_s,
            .sections = {
                {u"purpose"_s, u"Что проверяет инструмент"_s,
                 u"Tweakopedia запускает DISM /Online /Cleanup-Image /ScanHealth, а затем подтверждает состояние командой /CheckHealth. Проверяется хранилище компонентов текущей установленной Windows, а не личные файлы пользователя."_s},
                {u"result"_s, u"Как понимать результат"_s,
                 u"Исправное состояние означает, что DISM не обнаружил признаков повреждения. Восстанавливаемое повреждение означает, что найдены отсутствующие или изменённые компоненты и Windows допускает их восстановление. Невосстанавливаемое состояние требует изучения журнала и отдельного источника файлов."_s},
                {u"repair"_s, u"Что делать при повреждении"_s,
                 u"Используйте кнопку «Восстановить». Будет выполнена команда /RestoreHealth, после которой Tweakopedia повторно запустит /CheckHealth. Не прерывайте процесс: отдельные проценты могут долго не меняться."_s},
                {u"after"_s, u"После восстановления"_s,
                 u"Повторите проверку DISM. Если неполадки Windows сохраняются, откройте Терминал от имени администратора и выполните sfc /scannow: SFC использует исправленное хранилище как источник системных файлов."_s},
                {u"failure"_s, u"Если восстановление не помогло"_s,
                 u"Откройте журнал DISM и сохраните код завершения. Частые причины — недоступный источник компонентов, несоответствие версии исходных файлов или незавершённые операции обслуживания. Для /Source используйте образ той же редакции, языка, архитектуры и ветки Windows."_s},
                {u"technical"_s, u"Технические сведения"_s,
                 u"Проверка: DISM.exe /Online /Cleanup-Image /ScanHealth и /CheckHealth. Восстановление: DISM.exe /Online /Cleanup-Image /RestoreHealth с последующим /CheckHealth. Полный вывод сохраняется в журнале сеанса; системный журнал DISM обычно расположен в %WINDIR%\\Logs\\DISM\\dism.log."_s, true},
            },
            .technicalObjects = {u"DISM.exe"_s, u"%WINDIR%\\WinSxS"_s,
                                 u"%WINDIR%\\Logs\\DISM\\dism.log"_s},
            .relatedArticleIds = {u"guide.file-system-check"_s,
                                  u"guide.network-diagnostics"_s},
        },
        {
            .id = u"guide.file-system-check"_s,
            .title = u"Проверка файловой системы с помощью CHKDSK"_s,
            .summary = u"Что проверяет режим только чтения, как понимать найденные ошибки NTFS и когда переходить к исправлению."_s,
            .purpose = u"CHKDSK проверяет структуру выбранного тома: записи файлов, индексы каталогов, дескрипторы и сведения о свободном пространстве."_s,
            .categoryId = u"diagnostic-guides"_s,
            .categoryTitle = u"Диагностика и восстановление"_s,
            .subcategoryId = u"file-system"_s,
            .subcategoryTitle = u"Файловая система"_s,
            .sections = {
                {u"purpose"_s, u"Что проверяет инструмент"_s,
                 u"Tweakopedia запускает chkdsk.exe для выбранного локального тома NTFS без ключей исправления. Команда читает метаданные файловой системы и сообщает результат, не изменяя содержимое тома."_s},
                {u"result"_s, u"Что означают найденные ошибки"_s,
                 u"Логические ошибки указывают на несогласованные записи NTFS, индексы или карту свободного пространства. Сообщения о нечитаемых участках и повреждённых секторах могут указывать на проблемы самого накопителя либо соединения с ним."_s},
                {u"before-repair"_s, u"Перед исправлением"_s,
                 u"Скопируйте важные файлы на другой физический накопитель. Повторяющиеся ошибки после исправления — повод проверить состояние устройства и соединения, а не многократно запускать восстановление файловой системы."_s},
                {u"repair"_s, u"Исправление логических ошибок"_s,
                 u"В Терминале администратора выполните chkdsk X: /f, заменив X буквой тома. Если том используется системой, Windows предложит выполнить проверку при следующей перезагрузке. Ключ /r дополнительно ищет нечитаемые участки и может работать много часов."_s},
                {u"after"_s, u"После исправления"_s,
                 u"Повторите проверку без ключей. Если CHKDSK снова находит ошибки, изучите состояние накопителя, кабеля или контроллера и перенесите данные до дальнейших экспериментов."_s},
                {u"technical"_s, u"Технические сведения"_s,
                 u"Проверка Tweakopedia: chkdsk.exe X:. Код 0 означает, что ошибок не найдено; код 2 — ошибки найдены, но не исправлены; код 3 — проверка не завершена. Исправление /f и поиск нечитаемых участков /r намеренно не запускаются автоматически."_s, true},
            },
            .technicalObjects = {u"chkdsk.exe"_s, u"NTFS"_s,
                                 u"chkdsk X: /f"_s, u"chkdsk X: /r"_s},
            .relatedArticleIds = {u"guide.system-integrity"_s,
                                  u"guide.network-diagnostics"_s},
        },
        {
            .id = u"guide.network-diagnostics"_s,
            .title = u"Диагностика сетевой конфигурации Windows"_s,
            .summary = u"Как интерпретировать результаты проверки адаптеров, DNS, прокси, hosts, маршрутов и Windows Firewall."_s,
            .purpose = u"Инструмент собирает связанные сетевые параметры в одном отчёте, чтобы показать отклонения и дать пользователю понятную точку для дальнейшей проверки."_s,
            .categoryId = u"diagnostic-guides"_s,
            .categoryTitle = u"Диагностика и восстановление"_s,
            .subcategoryId = u"network"_s,
            .subcategoryTitle = u"Сеть"_s,
            .sections = {
                {u"scope"_s, u"Что входит в проверку"_s,
                 u"Проверяются активные сетевые адаптеры и адреса, DNS-серверы, пользовательский и системный прокси, активные строки hosts, маршруты по умолчанию, профили Windows Firewall и пользовательские разрешающие входящие правила."_s},
                {u"statuses"_s, u"Как понимать статусы"_s,
                 u"«Замечаний нет» означает, что проверка получила ожидаемый результат. «Требует внимания» обозначает конфигурацию, которую стоит сопоставить с вашей сетью и установленными программами. «Данные недоступны» означает, что команда не вернула достаточных сведений; это не равнозначно поломке."_s},
                {u"inspect"_s, u"Как изучать результат"_s,
                 u"Выберите пункт в левом списке. В боковой панели будут показаны адреса, интерфейсы, маршруты или свойства правил. Сопоставьте имена программ и пути с тем, что установлено на компьютере и используется в вашей сети."_s},
                {u"actions"_s, u"Что можно изменить"_s,
                 u"Явно ненужное пользовательское правило Windows Firewall можно удалить из панели сведений; перед этим Tweakopedia экспортирует текущую политику. Для DNS, прокси, hosts и маршрутов сначала определите программу или администратора, создавших настройку, и только затем используйте восстановление сети."_s},
                {u"connection"_s, u"Если соединение не работает"_s,
                 u"Проверьте, получил ли активный адаптер адрес, шлюз и DNS по DHCP. Сопоставьте маршрут по умолчанию с используемым интерфейсом, временно исключите пользовательский прокси и повторите диагностику. Ошибка только одного сайта чаще связана с DNS, hosts или удалённой стороной, а не со всем адаптером."_s},
                {u"technical"_s, u"Технические сведения"_s,
                 u"Сведения собираются встроенными командами PowerShell: Get-NetAdapter, Get-NetIPConfiguration, Get-DnsClientServerAddress, Get-NetRoute, Get-NetFirewallProfile и Get-NetFirewallRule. Пользовательский прокси также читается из параметров WinINet, системный — из WinHTTP."_s, true},
            },
            .technicalObjects = {u"Get-NetIPConfiguration"_s,
                                 u"Get-DnsClientServerAddress"_s,
                                 u"netsh winhttp"_s, u"hosts"_s,
                                 u"Get-NetRoute"_s, u"Get-NetFirewallRule"_s},
            .relatedArticleIds = {u"guide.system-integrity"_s,
                                  u"guide.file-system-check"_s},
        },
    };
    return articles;
}

const EncyclopediaReferenceArticle* findEncyclopediaReferenceArticle(QStringView id)
{
    const auto& articles = encyclopediaReferenceArticles();
    const auto iterator = std::find_if(
        articles.cbegin(), articles.cend(),
        [&](const auto& article) { return article.id == id; });
    return iterator == articles.cend() ? nullptr : &*iterator;
}

content::CategoryDefinition encyclopediaReferenceCategory()
{
    return {
        .id = u"diagnostic-guides"_s,
        .title = u"Диагностика и восстановление"_s,
        .subcategories = {
            {.id = u"system-integrity"_s, .title = u"Целостность Windows"_s},
            {.id = u"file-system"_s, .title = u"Файловая система"_s},
            {.id = u"network"_s, .title = u"Сеть"_s},
        },
    };
}

} // namespace tweakopedia::app
