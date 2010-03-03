/*
  This file is part of the Grantlee template system.

  Copyright (c) 2009 Stephen Kelly <steveire@gmail.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License version 3 only, as published by the Free Software Foundation.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License version 3 for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "engine.h"
#include "engine_p.h"

#include <QRegExp>
#include <QTextStream>
#include <QDir>
#include <QPluginLoader>

#include "taglibraryinterface.h"
#include "template_p.h"
#include "templateloader.h"
#include "grantlee_version.h"
#include "exception.h"

using namespace Grantlee;

static const char * __scriptableLibName = "grantlee_scriptabletags";

class ScriptableLibraryContainer : public TagLibraryInterface
{
public:
  ScriptableLibraryContainer( QHash<QString, AbstractNodeFactory*> factories, QHash<QString, Filter *> filters )
      : m_nodeFactories( factories ), m_filters( filters ) {

  }

  QHash<QString, AbstractNodeFactory*> nodeFactories( const QString &name = QString() ) {
    Q_UNUSED( name );
    return m_nodeFactories;
  }

  QHash<QString, Filter*> filters( const QString &name = QString() ) {
    Q_UNUSED( name );
    return m_filters;
  }

private:
  QHash<QString, AbstractNodeFactory*> m_nodeFactories;
  QHash<QString, Filter*> m_filters;

};

Engine::Engine( QObject *parent )
    : QObject( parent ), d_ptr( new EnginePrivate( this ) )
{
  d_ptr->m_defaultLibraries << "grantlee_defaulttags"
                            << "grantlee_loadertags"
                            << "grantlee_defaultfilters"
                            << "grantlee_scriptabletags";
}

Engine::~Engine()
{
  qDeleteAll( d_ptr->m_scriptableLibraries );
  d_ptr->m_libraries.clear();
  foreach(QPluginLoader *pluginLoader, d_ptr->m_pluginLoaders)
    pluginLoader->unload();
  qDeleteAll(d_ptr->m_pluginLoaders);
  delete d_ptr;
}

QList<AbstractTemplateLoader::Ptr> Engine::templateLoaders()
{
  Q_D( Engine );
  return d->m_loaders;
}

void Engine::addTemplateLoader( AbstractTemplateLoader::Ptr loader )
{
  Q_D( Engine );
  d->m_loaders << loader;
}

QString Engine::mediaUri( const QString &fileName ) const
{
  Q_D( const Engine );
  QListIterator<AbstractTemplateLoader::Ptr> it( d->m_loaders );

  QString uri;
  while ( it.hasNext() ) {
    AbstractTemplateLoader::Ptr loader = it.next();
    uri = loader->getMediaUri( fileName );
    if ( !uri.isEmpty() )
      break;
  }
  return uri;
}

void Engine::setPluginDirs( const QStringList &dirs )
{
  Q_D( Engine );
  d->m_pluginDirs = dirs;
}

QStringList Engine::defaultLibraries() const
{
  Q_D( const Engine );
  return d->m_defaultLibraries;
}

void Engine::addDefaultLibrary( const QString &libName )
{
  Q_D( Engine );
  d->m_defaultLibraries << libName;
}

void Engine::removeDefaultLibrary( const QString &libName )
{
  Q_D( Engine );
  d->m_defaultLibraries.removeAll( libName );
}

void Engine::loadDefaultLibraries()
{
  Q_D( Engine );
  // Make sure we can load default scriptable libraries if we're supposed to.
  if ( d->m_defaultLibraries.contains( __scriptableLibName ) ) {
    TagLibraryInterface *library = d->loadCppLibrary( __scriptableLibName, GRANTLEE_VERSION_MINOR );
    if ( library )
    {
      library->setEngine( this );
    }
  }

  foreach( const QString &libName, d->m_defaultLibraries ) {
    if ( libName == __scriptableLibName )
      continue;

    TagLibraryInterface *library = loadLibrary( libName );
    if ( !library )
      continue;
  }
}

TagLibraryInterface* Engine::loadLibrary( const QString &name )
{
  Q_D( Engine );

  if ( name == __scriptableLibName )
    return 0;

  // already loaded by the engine.
  if ( d->m_libraries.contains( name ) )
    return d->m_libraries.value( name );

  uint minorVersion = GRANTLEE_VERSION_MINOR;
  while ( minorVersion >= GRANTLEE_MIN_PLUGIN_VERSION )
  {
    TagLibraryInterface* library = d->loadLibrary( name, minorVersion-- );
    if ( library )
      return library;
  }
  return 0;
}

TagLibraryInterface* EnginePrivate::loadLibrary( const QString &name, uint minorVersion )
{
  TagLibraryInterface* scriptableLibrary = loadScriptableLibrary( name, minorVersion );
  if ( scriptableLibrary )
    return scriptableLibrary;

  // else this is not a scriptable library.

  return loadCppLibrary( name, minorVersion );
}

EnginePrivate::EnginePrivate( Engine *engine )
  : q_ptr( engine )
{
}

TagLibraryInterface* EnginePrivate::loadScriptableLibrary( const QString &name, uint minorVersion )
{
  int pluginIndex = 0;
  QString libFileName;
  if ( !m_libraries.contains( __scriptableLibName ) )
    return 0;

  while ( m_pluginDirs.size() > pluginIndex ) {
    QString nextDir = m_pluginDirs.at( pluginIndex++ );
    libFileName = nextDir + QString( "/%1.%2" ).arg( GRANTLEE_VERSION_MAJOR ).arg( minorVersion ) + '/' + name + ".qs";
    QFile file( libFileName );
    if ( !file.exists() )
      continue;

    TagLibraryInterface *scriptableTagLibrary = m_libraries.value( __scriptableLibName );

    QHash<QString, AbstractNodeFactory*> factories = scriptableTagLibrary->nodeFactories( libFileName );
    QHash<QString, Filter*> filters = scriptableTagLibrary->filters( libFileName );

    TagLibraryInterface *library = new ScriptableLibraryContainer( factories, filters );
    m_scriptableLibraries << library;
    return library;
  }
  return 0;
}

TagLibraryInterface* EnginePrivate::loadCppLibrary( const QString &name, uint minorVersion )
{
  Q_Q( Engine );
  int pluginIndex = 0;
  QString libFileName;

  QObject *plugin = 0;
  while ( m_pluginDirs.size() > pluginIndex ) {
    QString nextDir = m_pluginDirs.at( pluginIndex++ );
    QDir pluginDir( nextDir + QString( "/%1.%2" ).arg( GRANTLEE_VERSION_MAJOR ).arg( minorVersion ) + '/' );

    if ( !pluginDir.exists() )
      continue;

    QStringList list = pluginDir.entryList( QStringList( name + "*" ) );

    if (list.isEmpty())
      continue;

    QPluginLoader *loader = new QPluginLoader( pluginDir.absoluteFilePath( list.first() ) );

    plugin = loader->instance();
    m_pluginLoaders.append( loader );
    if ( plugin )
      break;
  }
  if ( !plugin )
    return 0;

  TagLibraryInterface *library = qobject_cast<TagLibraryInterface*>( plugin );
  library->setEngine( q );
  m_libraries.insert( name, library );
  return library;
}

Template Engine::loadByName( const QString &name ) const
{
  Q_D( const Engine );

  QListIterator<AbstractTemplateLoader::Ptr> it( d->m_loaders );
  while ( it.hasNext() ) {
    AbstractTemplateLoader::Ptr loader = it.next();

    if ( !loader->canLoadTemplate( name ) )
      continue;

    Template t = loader->loadByName( name, this );

    if ( t ) {
      return t;
    }
  }
  return Template();
}

MutableTemplate Engine::loadMutableByName( const QString &name ) const
{
  Q_D( const Engine );

  QListIterator<AbstractTemplateLoader::Ptr> it( d->m_loaders );

  while ( it.hasNext() ) {
    AbstractTemplateLoader::Ptr loader = it.next();
    MutableTemplate t = loader->loadMutableByName( name, this );
    if ( t ) {
      return t;
    }
  }
  throw Grantlee::Exception( TagSyntaxError, QString( "Most recent state is invalid." ) );
}

MutableTemplate Engine::newMutableTemplate( const QString &content, const QString &name ) const
{
  MutableTemplate t = MutableTemplate( new MutableTemplateImpl( this ) );
  t->setObjectName( name );
  t->setContent( content );
  return t;
}

Template Engine::newTemplate( const QString &content, const QString &name ) const
{
  Template t = Template( new TemplateImpl( this ) );
  t->setObjectName( name );
  t->setContent( content );
  return t;
}

#include "engine.moc"