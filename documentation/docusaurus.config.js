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
  favicon: 'img/favicon.ico',
  organizationName: 'SNMetamorph', // Usually your GitHub org/user name.
  projectName: 'PrimeXT', // Usually your repo name.

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
      }),
    ],
  ],

  themeConfig:
    /** @type {import('@docusaurus/preset-classic').ThemeConfig} */
    ({
      navbar: {
        title: 'PrimeXT Docs',
        logo: {
          alt: 'PrimeXT Logo',
          src: 'img/logo.png',
        },
        items: [
          {
            type: 'doc',
            docId: 'intro',
            position: 'left',
            label: 'Tutorial',
          },
          {to: '/blog', label: 'Blog', position: 'left'},
          {
            href: 'https://github.com/facebook/docusaurus',
            label: 'GitHub',
            position: 'right',
          },
        ],
      },
      footer: {
        style: 'dark',
        links: [
          {
            title: 'Docs',
            items: [
              {
                label: 'Tutorial',
                to: '/docs/intro',
              },
            ],
          },
          {
            title: 'Community',
            items: [
              {
                label: 'GitHub',
                href: 'https://github.com/SNMetamorph/PrimeXT',
              },
              {
                label: 'Discord',
                href: 'https://discord.gg/BxQUMUescJ',
              },
              {
                label: 'VKontakte',
                href: 'https://vk.com/xash3d.modding',
              },
            ],
          },
          {
            title: 'More',
            items: [
              {
                label: 'HLFX Topic',
                href: 'https://hlfx.ru/forum/showthread.php?s=&threadid=5371',
              },
              {
                label: 'Xash3D FWGS GitHub',
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
    }),
};

module.exports = config;
