#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    model = nullptr;

    model = new QStandardItemModel();
    ui->tableView->setModel(model);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    //вставляем 6 столбцов
    model->insertColumns(0, 8);
    model->setHeaderData(0, Qt::Horizontal, "Таблица");
    model->setHeaderData(1, Qt::Horizontal, "Поле");
    model->setHeaderData(2, Qt::Horizontal, "ID(0)");
    model->setHeaderData(3, Qt::Horizontal, "Значение");
    model->setHeaderData(4, Qt::Horizontal, "1й символ");
    model->setHeaderData(5, Qt::Horizontal, "Посл символ");
    model->setHeaderData(6, Qt::Horizontal, "Таблица поиска");
    model->setHeaderData(7, Qt::Horizontal, "Id поиска");

    int rowItr = 0;

    if (QSqlDatabase::isDriverAvailable("QPSQL")){
        db = QSqlDatabase::addDatabase("QPSQL");
        db.setHostName("localhost");
        db.setPort(5433);
        db.setDatabaseName("testDB");
        db.setUserName("postgres");
        db.setPassword("root");
    }else{
        qDebug()<<"Не найден драйвер PSQL\n";
        QMetaObject::invokeMethod(this, "close", Qt::QueuedConnection);
        return;
    }

    if (!db.open()){
        qDebug()<<"Соединение с базой данных не установлено\n";
        QMessageBox::critical(this, "Ошибка", "Ошибка соединения с базой. "
                                              "Ошибка SQL: "+db.lastError().text());

        QMetaObject::invokeMethod(this, "close", Qt::QueuedConnection);
        return;
    }else{
        qDebug()<<"Соединение с базой данных установлено\n";
    }

   //выбор нужных таблиц
    QSqlQuery queryGetTables(db);
    //подготовили шаблон
    queryGetTables.prepare("SELECT tablename FROM pg_tables WHERE tablename NOT LIKE 'pg\\_%' "
                           "AND tablename NOT LIKE 'sql\\_%'");

    if(queryGetTables.exec()){
        while(queryGetTables.next()){
            QString tmpTable = queryGetTables.value("tablename").toString();

            QSqlQuery queryGetCols(db);
            queryGetCols.prepare(QString("SELECT attname FROM pg_attribute, pg_type "
                                 "WHERE typname = '%1' AND attrelid = typrelid AND attname NOT IN ('cmin', 'cmax', 'ctid', 'oid', 'tableoid', 'xmin', 'xmax') "
                                         "AND atttypid=1043").arg(tmpTable)); //1043 - текстовые поля

            if(queryGetCols.exec()){
                while(queryGetCols.next()){
                    QString tmpField = queryGetCols.value("attname").toString();

                    //проверка последнего и первого символа
                    QSqlQuery queryFindSpecSymbols(db);
                    queryFindSpecSymbols.prepare("SELECT * FROM \""+tmpTable+"\" WHERE ("
                                                 "(\""+tmpField+"\") NOT SIMILAR TO '%([«~&\r\t\n»_%}\/@{;:ёЁ•=+a-zA-Zа-яА-Я0-9()?!,.\" #*№</>-])' "
                                                 "OR "
                                                 "(\""+tmpField+"\") NOT SIMILAR TO '([«~&\r\t\n»_%}\/@{;:ёЁ•=+a-zA-Zа-яА-Я0-9()?!,.\" #*№</>-])%' "
                                                 "OR "  //FULL
                                                 "(\""+tmpField+"\") NOT SIMILAR TO '([«~&\r\t\n»_%}\/@{;:ёЁ•=+a-zA-Zа-яА-Я0-9()?!,.\" #*№</>-])+' " //FULL
                                                 ") "
                                                 "AND \""+tmpField+"\"!=''");

                    if(queryFindSpecSymbols.exec()){
                        while(queryFindSpecSymbols.next()){
                            QString tmpID = queryFindSpecSymbols.record().field(0).name();
                            QString tmpValue = queryFindSpecSymbols.value(tmpField).toString();
                            //нужно взять tmpIDvalue и tmpTable
                            //если находим неверный символ в строке, то переходим в таблицу , по которой  производим поиск
                            //ищем поле с нужным названием (как это сделать?) и нужным значением
                            //выводим IdValue строки ,по которой будем осуществлять поиск
                            //чтобы найти код в таблице, нужно понять, какие есть зависимые таблицы  и от каких зависит таблица
                            //pg_attrdef.adrelid - таблица к которой принадлежит столбец
                            QString tmpIDvalue = queryFindSpecSymbols.value(tmpID).toString();//Находим ID строки, где некорректный символ

                            QChar firstChar = tmpValue.at(0);
                            QChar lastChar = tmpValue.at(tmpValue.size()-1);

                            //если последний символ []\^ то проверки нет
                            if(lastChar!='[' && lastChar!=']' && lastChar!='^' && lastChar!='\\'){
                                qDebug()<<"TABLE: "<<tmpTable<<" FIELD: "<<tmpField<<" ID: "<<tmpID<<" ID_value "<<tmpIDvalue<<" VALUE: "<<tmpValue<<"Char: "<<lastChar;
                          //Строки, в которые закидываем таблицу и id
                           QString find_id;
                           QString find_table="";
                                if((tmpTable.startsWith("u_"))||(tmpTable.startsWith("tw_"))){
                                QSqlQuery queryFindTable_Id;
                                if(tmpTable=="u_LinWU"){
                                    queryFindTable_Id.prepare("SELECT l_code FROM  \"u_LinWU\" WHERE l_code = :id ");
                                    queryFindTable_Id.bindValue(":id",queryFindSpecSymbols.value(tmpID).toInt());
                                }
                                    else if (tmpTable=="u_Mag") {
                                        queryFindTable_Id.prepare("SELECT l_code FROM \"u_LinWU\" WHERE l_child = :id");
                                        queryFindTable_Id.bindValue(0,queryFindSpecSymbols.value(tmpID).toInt());
                                    }
                                        else if (tmpTable=="tw_size") {
                                            queryFindTable_Id.prepare("SELECT \"lwc_linWU\" FROM \"u_Config\" WHERE lwc_detail = :id LIMIT 1");
                                            queryFindTable_Id.bindValue(0,queryFindSpecSymbols.value(tmpID).toInt());
                                        }
                                            else  if(tmpTable=="u_WU"){
                                                queryFindTable_Id.prepare("SELECT l_code FROM \"u_LinWU\" WHERE \"l_WU\" = :id");
                                                queryFindTable_Id.bindValue(0,queryFindSpecSymbols.value(tmpID).toInt());
                                                }
                                if(!queryFindTable_Id.exec()){
                                    qDebug()<<queryFindTable_Id.lastError().text();
                                    }else {
                                        while(queryFindTable_Id.next())
                                        find_id = queryFindTable_Id.value(queryFindTable_Id.record().field(0).name()).toString();
                                        find_table ="u_LinWU";
                                    }
                            }
                                if(tmpTable.startsWith("o_")){
                                QSqlQuery queryFindTable_Id;
                                    if(tmpTable=="o_Abonent"){
                                        queryFindTable_Id.prepare("SELECT p_code FROM  \"o_Abonent\" WHERE p_code = :id ");
                                        queryFindTable_Id.bindValue(0,queryFindSpecSymbols.value(tmpID).toInt());
                                    }
                                        else if (tmpTable=="o_Object") {
                                            queryFindTable_Id.prepare("SELECT o_abon FROM \"o_Object\" WHERE o_code = :id LIMIT 1");
                                            queryFindTable_Id.bindValue(0,queryFindSpecSymbols.value(tmpID).toInt());
                                        }
                                if(!queryFindTable_Id.exec()){
                                    qDebug()<<queryFindTable_Id.lastError().text();
                                }else {
                                    while(queryFindTable_Id.next())
                                    find_id = queryFindTable_Id.value(queryFindTable_Id.record().field(0).name()).toString();
                                    find_table ="o_Abonent";
                                    }
                               }

                                if(tmpTable.startsWith("n_")){
                                QSqlQuery queryFindTable_Id;
                                    if(tmpTable=="n_Order"){
                                        queryFindTable_Id.prepare("SELECT na_code FROM  \"n_Order\" WHERE na_code = :id ");
                                        queryFindTable_Id.bindValue(0,queryFindSpecSymbols.value(tmpID).toInt());
                                    }
                                if(!queryFindTable_Id.exec()){
                                    qDebug()<<queryFindTable_Id.lastError().text();
                                }else {
                                    while(queryFindTable_Id.next())
                                    find_id = queryFindTable_Id.value(queryFindTable_Id.record().field(0).name()).toString();
                                    find_table ="n_Order";
                                 }
                                }

                                if(tmpTable.startsWith("d_")){
                                QSqlQuery queryFindTable_Id;
                                    if(tmpTable=="d_Act"){
                                        queryFindTable_Id.prepare("SELECT ac_code FROM  \"d_Act\" WHERE ac_code = :id ");
                                        queryFindTable_Id.bindValue(0,queryFindSpecSymbols.value(tmpID).toInt());
                                    }
                                        else if(tmpTable=="d_ActLin"){
                                            queryFindTable_Id.prepare("SELECT \"acl_Act\" FROM  \"d_ActLin\" WHERE \"acl_Act\" = :id LIMIT 1 ");
                                            queryFindTable_Id.bindValue(0,queryFindSpecSymbols.value(tmpID).toInt());
                                        }
                                            else if(tmpTable=="d_Insp"){
                                                queryFindTable_Id.prepare("SELECT ac_code FROM \"d_Act\" WHERE ac_insp = :id LIMIT 1 ");
                                                queryFindTable_Id.bindValue(0,queryFindSpecSymbols.value(tmpID).toInt());
                                            }
                                                else if(tmpTable=="d_Stamp"){
                                                    queryFindTable_Id.prepare("SELECT s_act FROM \"d_Stamp\" WHERE s_act = :id LIMIT 1 ");
                                                    queryFindTable_Id.bindValue(0,queryFindSpecSymbols.value(tmpID).toInt());
                                                }
                                                    else if(tmpTable=="d_TypeWC"){
                                                        queryFindTable_Id.prepare("SELECT \"acl_Act\" FROM  \"d_ActLin\" WHERE \"acl_WS\" = :id LIMIT 1 ");
                                                        queryFindTable_Id.bindValue(0,queryFindSpecSymbols.value(tmpID).toInt());
                                                    }
                                if(!queryFindTable_Id.exec()){
                                    qDebug()<<queryFindTable_Id.lastError().text();
                                }else {
                                    while(queryFindTable_Id.next())
                                    find_id = queryFindTable_Id.value(queryFindTable_Id.record().field(0).name()).toString();
                                    find_table ="d_Act";
                                 }
                                }

                                if(tmpTable.startsWith("a_")){
                                QSqlQuery queryFindTable_Id;
                                    if(tmpTable=="a_Adr_Bild"){
                                        queryFindTable_Id.prepare("SELECT a_code FROM  \"a_Adr_Bild\" WHERE a_code = :id ");
                                        queryFindTable_Id.bindValue(0,queryFindSpecSymbols.value(tmpID).toInt());
                                    }
                                        else if(tmpTable=="a_Bilding"){
                                            queryFindTable_Id.prepare("SELECT a_code FROM  \"a_Adr_Bild\" WHERE a_bild = :id LIMIT 1 ");
                                            queryFindTable_Id.bindValue(0,queryFindSpecSymbols.value(tmpID).toInt());
                                        }
                                            else if(tmpTable=="a_Complex"){
                                                QSqlQuery queryPrefinderTable;
                                                QString comp_id;
                                                queryPrefinderTable.prepare("SELECT c_code FROM \"a_Complex\" WHERE c_code = :id ");
                                                queryPrefinderTable.bindValue(0,queryFindSpecSymbols.value(tmpID).toInt());
                                                if(queryPrefinderTable.exec()){
                                                    while(queryPrefinderTable.next())
                                                    {
                                                        comp_id = queryPrefinderTable.value(queryPrefinderTable.record().field(0).name()).toString();
                                                    }
                                                }
                                                    queryFindTable_Id.prepare("SELECT a_code FROM \"a_Adr_Bild\",\"a_Bilding\" "
                                                                              "WHERE \"a_Adr_Bild\".a_bild = \"a_Bilding\".b_code "
                                                                              "AND \"a_Bilding\".b_compl = :id ");
                                                    queryFindTable_Id.bindValue(0,comp_id.toInt());
                                            }
                                                else if(tmpTable=="a_Street"){
                                                    queryFindTable_Id.prepare("SELECT a_code FROM \"a_Adr_Bild\" WHERE a_street = :id LIMIT 1 ");
                                                    queryFindTable_Id.bindValue(0,queryFindSpecSymbols.value(tmpID).toInt());
                                                }
                                                    else if(tmpTable=="d_TypeWC"){
                                                        queryFindTable_Id.prepare("SELECT \"acl_Act\" FROM  \"d_ActLin\" WHERE \"acl_WS\" = :id LIMIT 1 ");
                                                        queryFindTable_Id.bindValue(0,queryFindSpecSymbols.value(tmpID).toInt());
                                                    }
                                if(!queryFindTable_Id.exec()){
                                    qDebug()<<queryFindTable_Id.lastError().text();
                                }else {
                                    while(queryFindTable_Id.next())
                                    find_id = queryFindTable_Id.value(queryFindTable_Id.record().field(0).name()).toString();
                                    find_table ="a_Adr_Bild";
                                 }
                                }
                                model->insertRow(rowItr,QList<QStandardItem*>()<<
                                                 new QStandardItem(tmpTable)<<
                                                 new QStandardItem(tmpField)<<
                                                 new QStandardItem(tmpIDvalue)<<
                                                 new QStandardItem(tmpValue)<<
                                                 new QStandardItem(firstChar)<<
                                                 new QStandardItem(lastChar)<<
                                                 new QStandardItem(find_table)<<
                                                 new QStandardItem(find_id)
                                                 );
                            }
                        }
                    }else{
                        qDebug()<<queryFindSpecSymbols.lastError().text();
                    }
                }
            }
        }
    }

    ui->tableView->resizeColumnsToContents();
    ui->tableView->resizeRowsToContents();
}

MainWindow::~MainWindow()
{
    if(model!=nullptr){
        delete model;
        model = nullptr;
    }
    delete ui;
}
