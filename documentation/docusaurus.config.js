// @ts-check
// Note: type annotations allow type checking and IDEs autocompletion

const lightCodeTheme = require('prism-react-renderer/themes/github');
const darkCodeTheme = require('prism-react-renderer/themes/dracula');

/** @type {import('@docusaurus/types').Config} */
const config = {
  title: 'PrimeXT Documentation',
  tagline: 'New stage of Half-Life 1 modding',
  url: 'https://snmetamorph.github.io',
  baseUrl: '/PrimeXT/',
  onBrokenLinks: 'throw',
  onBrokenMarkdownLinks: 'warn',
  favicon: 'img/favicon.ico?v=2',
  organizationName: 'SNMetamorph', // Usually your GitHub org/user name.
  projectName: 'PrimeXT', // Usually your repo name.
  plugins: [],
  themes: [
    [
      require.resolve('@easyops-cn/docusaurus-search-local'),
      /** @type {import('@easyops-cn/docusaurus-search-local').PluginOptions} */
      ({
        // `hashed` is recommended as long-term-cache of index file is possible.
        hashed: true,
        language: ["en", "ru"],
      }),
    ],
  ],

  presets: [
    [
      'classic',
      /** @type {import('@docusaurus/preset-classic').Options} */
      ({
        docs: {
          sidebarPath: require.resolve('./sidebars.js'),
          editUrl: 'https://github.com/SNMetamorph/PrimeXT/tree/master/documentation',
        },
        blog: {
          showReadingTime: true,
          editUrl: 'https://github.com/SNMetamorph/PrimeXT/tree/master/documentation',
        },
        theme: {
          customCss: require.resolve('./src/css/custom.css'),
        },
        gtag: {
          trackingID: 'G-NLS6RNCBDY',
          anonymizeIP: false,
        },
        sitemap: {
          changefreq: 'daily',
          priority: 0.5,
          ignorePatterns: [
            '/PrimeXT/search/**',
            '/PrimeXT/blog/tags/**'
          ],
        },
      }),
    ],
  ],

  themeConfig:
    /** @type {import('@docusaurus/preset-classic').ThemeConfig} */
    ({
      navbar: {
        title: 'PrimeXT',
        logo: {
          alt: 'PrimeXT Logo',
          src: 'img/logo_256.png',
        },
        items: [
          {
            type: 'doc',
            docId: 'eng/intro',
            position: 'left',
            label: 'Documentation',
          },
          {
            href: 'https://github.com/SNMetamorph/PrimeXT',
            label: 'GitHub',
            position: 'right',
          },
        ],
      },
      footer: {
        style: 'dark',
        links: [
          {
            title: 'Community',
            items: [
              {
                label: 'Discord',
                href: 'https://discord.gg/BxQUMUescJ',
              },
              {
                label: 'ModDB',
                href: 'https://www.moddb.com/mods/primext',
              },
              {
                label: 'VKontakte',
                href: 'https://vk.com/xash3d.modding',
              },
            ],
          },
          {
            title: 'Forums',
            items: [
              {
                label: 'HLFX Topic',
                href: 'https://hlfx.ru/forum/showthread.php?s=&threadid=5371',
              },
              {
                label: 'CSM.Dev Topic',
                href: 'https://csm.dev/threads/primext.39978/',
              },
            ],
          },
          {
            title: 'Related projects',
            items: [
              {
                label: 'Xash3D FWGS',
                href: 'https://github.com/FWGS/xash3d-fwgs',
              },
            ],
          },
        ],
        copyright: `Copyright © ${new Date().getFullYear()} PrimeXT, Inc. Built with Docusaurus.`,
      },
      prism: {
        theme: lightCodeTheme,
        darkTheme: darkCodeTheme,
      },
      metadata: [
        { name: 'keywords', content: 'hlsdk, gamedev, programming, open source, half-life, xash3d, fwgs, goldsrc, sdk, opengl, physx, xashxt, modding, snmetamorph' }
      ]
    }),
};

module.exports = config;
